module;
#include <spp/analyse/macros.hpp>
#include <opex/macros.hpp>

module spp.analyse.utils.mem_utils;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.utils.ast_utils;
import genex;
import opex.cast;


auto spp::analyse::utils::mem_utils::memory_region_overlap(
    asts::Ast const &ast_1,
    asts::Ast const &ast_2)
    -> bool {
    const auto s1 = static_cast<std::string>(ast_1);
    const auto s2 = static_cast<std::string>(ast_2);
    return s1.starts_with(s2) or s2.starts_with(s1);
}


auto spp::analyse::utils::mem_utils::memory_region_right_overlap(
    asts::Ast const &ast_1,
    asts::Ast const &ast_2) -> bool {
    const auto s1 = static_cast<std::string>(ast_1);
    const auto s2 = static_cast<std::string>(ast_2);
    return s2.starts_with(s1);
}


auto spp::analyse::utils::mem_utils::validate_symbol_memory(
    asts::ExpressionAst &value_ast,
    asts::Ast const &move_ast,
    scopes::ScopeManager &sm,
    const bool check_move,
    const bool check_partial_move,
    const bool check_move_from_borrowed_ctx,
    const bool check_pins,
    const bool check_linked_pins,
    const bool mark_moves,
    asts::meta::CompilerMetaData *meta) -> void {
    // For tuple and array literals, recursively analyse each element.
    if (auto const *arr_literal = value_ast.to<asts::ArrayLiteralRepeatedElementAst>(); arr_literal != nullptr) {
        const auto x = arr_literal->elem.get();
        validate_symbol_memory(*x, move_ast, sm, true, true, true, true, true, mark_moves, meta);
        return;
    }
    if (auto const *arr_literal = value_ast.to<asts::ArrayLiteralExplicitElementsAst>(); arr_literal != nullptr) {
        for (auto &&x: arr_literal->elems) {
            validate_symbol_memory(*x, move_ast, sm, true, true, true, true, true, mark_moves, meta);
        }
        return;
    }
    if (auto const *tup_literal = value_ast.to<asts::TupleLiteralAst>(); tup_literal != nullptr) {
        for (auto &&x: tup_literal->elems) {
            validate_symbol_memory(*x, move_ast, sm, true, true, true, true, true, mark_moves, meta);
        }
        return;
    }

    // Get the symbol representing the outermost part of the expression being moved. Non-symbolic => temporary value.
    auto [var_sym, var_scope] = sm.current_scope->get_var_symbol_outermost(value_ast);
    if (var_sym == nullptr) { return; }
    const auto copies = var_scope->get_type_symbol(var_sym->type)->is_copyable();
    const auto partial_copies = var_scope->get_type_symbol(value_ast.infer_type(&sm, meta))->is_copyable();

    // Check for inconsistent memory moving (from branching).
    if (check_move and var_sym->memory_info->is_inconsistently_moved.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_moved;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyInitializedMemoryUseError>()
            .with_args(value_ast, *std::get<0>(pair), *std::get<1>(pair), "moved")
            .raises_from(sm.current_scope);
    }

    // Check for inconsistent memory initialization (from branching).
    if (check_move and var_sym->memory_info->is_inconsistently_initialized.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_initialized;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyInitializedMemoryUseError>()
            .with_args(value_ast, *std::get<0>(pair), *std::get<1>(pair), "initialized")
            .raises_from(sm.current_scope);
    }

    // Check for inconsistent partial memory moving (from branching).
    if (check_move and var_sym->memory_info->is_inconsistently_partially_moved.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_partially_moved;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyInitializedMemoryUseError>()
            .with_args(value_ast, *std::get<0>(pair), *std::get<1>(pair), "partially moved")
            .raises_from(sm.current_scope);
    }

    // Check for inconsistent pinned memory (from branching).
    if (check_pins and var_sym->memory_info->is_inconsistently_pinned.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_pinned;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyPinnedMemoryUseError>()
            .with_args(value_ast, *std::get<0>(pair), *std::get<1>(pair))
            .raises_from(sm.current_scope);
    }

    // Check the symbol hasn't already been moved.
    if (check_move and std::get<0>(var_sym->memory_info->ast_moved) != nullptr) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto [where_moved, _] = var_sym->memory_info->ast_moved;
        errors::SemanticErrorBuilder<errors::SppUninitializedMemoryUseError>()
            .with_args(value_ast, *where_init, *where_moved)
            .raises_from(sm.current_scope);
    }

    // Check the symbol doesn't have any outstanding partial moved (moving a partially moved object).
    if (check_partial_move and not var_sym->memory_info->ast_partial_moves.empty() and value_ast.to<asts::IdentifierAst>() != nullptr) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto where_pm = var_sym->memory_info->ast_partial_moves.front();
        errors::SemanticErrorBuilder<errors::SppPartiallyInitializedMemoryUseError>()
            .with_args(value_ast, *where_init, *where_pm)
            .raises_from(sm.current_scope);
    }

    // Check the symbol doesn't have any outstanding partial moves (directly moving a partial move).
    if (check_partial_move and not var_sym->memory_info->ast_partial_moves.empty() and value_ast.to<asts::IdentifierAst>() == nullptr) {
        const auto overlaps = var_sym->memory_info->ast_partial_moves
            | genex::views::filter([&value_ast](auto const &x) { return memory_region_right_overlap(*x, value_ast); })
            | genex::to<std::vector>();
        if (not overlaps.empty()) {
            const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
            const auto where_pm = overlaps.front();
            errors::SemanticErrorBuilder<errors::SppUninitializedMemoryUseError>()
                .with_args(value_ast, *where_init, *where_pm)
                .raises_from(sm.current_scope);
        }
    }

    // Check the symbol isn't being moved from a borrowed context.
    if (check_move_from_borrowed_ctx and std::get<0>(var_sym->memory_info->ast_borrowed) and value_ast.to<asts::IdentifierAst>() == nullptr and not partial_copies) {
        const auto [where_borrow, _] = var_sym->memory_info->ast_borrowed;
        const auto [where_pm, _] = var_sym->memory_info->ast_borrowed;
        errors::SemanticErrorBuilder<errors::SppMoveFromBorrowedMemoryError>()
            .with_args(value_ast, *where_pm, *where_borrow)
            .raises_from(sm.current_scope);
    }

    // Check the object being moved isn't pinned.
    const auto symbolic_pins = var_sym->memory_info->ast_pins
        | genex::views::cast_dynamic<asts::IdentifierAst const*>()
        | genex::to<std::vector>();

    const auto symbolic_pin_links = var_sym->memory_info->ast_linked_pins
        | genex::views::transform([](auto const &x) { return std::get<0>(x)->name; })
        | genex::to<std::vector>();

    if (check_pins and not symbolic_pins.empty() and not copies and not partial_copies) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto where_move = &move_ast;
        const auto where_pin = symbolic_pins.front();
        errors::SemanticErrorBuilder<errors::SppMoveFromPinnedMemoryError>()
            .with_args(value_ast, *where_init, *where_move, *where_pin)
            .raises_from(sm.current_scope);
    }

    if (check_linked_pins and not symbolic_pin_links.empty()) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto where_move = &move_ast;
        const auto where_pin = symbolic_pin_links.front().get();
        errors::SemanticErrorBuilder<errors::SppMoveFromPinLinkedMemoryError>()
            .with_args(value_ast, *where_init, *where_move, *where_pin)
            .raises_from(sm.current_scope);
    }

    // Mark the symbol as moved/partially-moved if it is not copyable.
    if (mark_moves and value_ast.to<asts::IdentifierAst>() != nullptr and not copies) {
        var_sym->memory_info->moved_by(value_ast, sm.current_scope);
    }

    else if (mark_moves and value_ast.to<asts::IdentifierAst>() == nullptr and not partial_copies) {
        var_sym->memory_info->ast_partial_moves.emplace_back(&value_ast);
    }
}


auto spp::analyse::utils::mem_utils::validate_inconsistent_memory(
    asts::Ast *parent,
    std::vector<asts::CaseExpressionBranchAst*> const &branches,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Define a simple alias for a list of symbols and their memory.
    using SymbolMemoryList = std::vector<std::pair<asts::CaseExpressionBranchAst*, mem_info_utils::MemoryInfoSnapshot>>;

    // Create a map of the symbols' memory  information before any branches are analysed.
    auto sym_mem_info = std::map<scopes::VariableSymbol*, SymbolMemoryList>();
    auto vs = sm->current_scope->all_var_symbols();
    auto pre_analysis_mem_info = vs
        | genex::views::transform([](auto const &x) { return std::make_pair(x.get(), x->memory_info->snapshot()); })
        | genex::to<std::vector>();

    for (auto &&branch : branches) {
        // Make a record of the symbols' memory status in the scope before the branch is analysed.
        auto old_symbol_mem_info = vs
            | genex::views::transform([](auto const &x) { return std::make_pair(x.get(), x->memory_info->snapshot()); })
            | genex::to<std::vector>();

        // Analyse the memory and then recheck the symbols' memory status.
        branch->stage_8_check_memory(sm, meta);
        auto new_symbol_mem_info = vs
            | genex::views::transform([](auto const &x) { return std::make_pair(x.get(), x->memory_info->snapshot()); })
            | genex::to<std::vector>();

        // Reset the memory status of the symbols for the next branch to analyse with the same original memory states.
        for (auto &&[sym, old_mem_status] : old_symbol_mem_info) {
            sym->memory_info->ast_initialization = {old_mem_status.ast_initialization, std::get<1>(sym->memory_info->ast_initialization)};
            sym->memory_info->ast_moved = {old_mem_status.ast_moved, std::get<1>(sym->memory_info->ast_moved)};
            sym->memory_info->ast_partial_moves = old_mem_status.ast_partial_moves;
            sym->memory_info->ast_pins = old_mem_status.ast_pins;
            sym->memory_info->initialization_counter = old_mem_status.initialization_counter;

            // Save this memory status for subsequent inter-branch status comparisons.
            auto new_symbol_mem_info_map = std::map(new_symbol_mem_info.begin(), new_symbol_mem_info.end());
            sym_mem_info[sym].emplace_back(branch, new_symbol_mem_info_map[sym]);
        }
    }

    // Add the pre-analysis memory states as a "final" branch (just for comparison purposes).
    for (auto &&[sym, mem_info_list] : pre_analysis_mem_info) {
        sym_mem_info[sym].emplace_back(nullptr, std::move(mem_info_list));
    }

    // Get the first "non-terminating" branch, and update the symbols to reflect its memory state.
    auto non_terminating_branch = genex::find_if(
        branches, [](auto const &x) { return not x->body->terminates(); });
    auto first_branch = non_terminating_branch == branches.end() ? parent : *non_terminating_branch;
    auto first_branch_index = non_terminating_branch != branches.end() ? genex::iterators::distance(branches.begin(), non_terminating_branch) as SSize : -1;
    auto first_branch_mem_info_getter = [&](auto const &branch_mem_info) {
        return first_branch_index != -1 ? branch_mem_info.at(first_branch_index as USize).second : branch_mem_info.back().second;
    };

    // Check for consistency among the branches' symbols' memory states.
    for (auto &&[sym, branches_memory_info_lists] : sym_mem_info) {
        auto first_branch_mem_info = first_branch_mem_info_getter(branches_memory_info_lists);

        // Assuming all new memory states are consistent across branches, update to the first "new" state list.
        sym->memory_info->ast_initialization = {first_branch_mem_info.ast_initialization, std::get<1>(sym->memory_info->ast_initialization)};
        sym->memory_info->ast_moved = {first_branch_mem_info.ast_moved, std::get<1>(sym->memory_info->ast_moved)};
        sym->memory_info->ast_partial_moves = first_branch_mem_info.ast_partial_moves;
        sym->memory_info->ast_pins = first_branch_mem_info.ast_pins;
        sym->memory_info->initialization_counter = first_branch_mem_info.initialization_counter;

        // Check the new memory status for each symbol is consistent across all branches that don't terminate.
        auto applicable_branch_memory_info_lists = branches_memory_info_lists
            | genex::views::drop_last(1)
            | genex::views::remove_if([&](auto const &x) { return x.first->body->terminates(); })
            | genex::to<std::vector>();

        for (auto const &[branch, branch_memory_info_list] : applicable_branch_memory_info_lists) {
            // Check for consistent initialization.
            if ((first_branch_mem_info.ast_initialization == nullptr) != (branch_memory_info_list.ast_initialization == nullptr)) {
                sym->memory_info->is_inconsistently_initialized = std::make_pair(first_branch, branch);
            }

            // Check for consistent moved state.
            if ((first_branch_mem_info.ast_moved == nullptr) != (branch_memory_info_list.ast_moved == nullptr)) {
                sym->memory_info->is_inconsistently_moved = std::make_pair(first_branch, branch);
            }

            // Check for consistent partial moves.
            if (first_branch_mem_info.ast_partial_moves != branch_memory_info_list.ast_partial_moves) {
                sym->memory_info->is_inconsistently_partially_moved = std::make_pair(first_branch, branch);
            }

            // Check for consistent pins.
            if (first_branch_mem_info.ast_pins != branch_memory_info_list.ast_pins) {
                sym->memory_info->is_inconsistently_pinned = std::make_pair(first_branch, branch);
            }
        }
    }
}
