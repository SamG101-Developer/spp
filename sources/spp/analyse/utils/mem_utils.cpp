module spp.analyse.utils.mem_utils;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.iter_expression_branch_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.utils.ast_utils;
import genex;


auto spp::analyse::utils::mem_utils::MemoryInfo::moved_by(
    asts::Ast const &ast, scopes::Scope *scope)

    -> void {
    ast_moved = {&ast, scope};
    ast_initialization = {nullptr, nullptr};
}


auto spp::analyse::utils::mem_utils::MemoryInfo::initialized_by(
    asts::Ast const &ast,
    scopes::Scope *scope)
    -> void {
    ast_initialization = {&ast, scope};
    ast_initialization_origin = {&ast, scope};
    ast_moved = {nullptr, nullptr};
    initialization_counter += 1;

    is_inconsistently_initialized = std::nullopt;
    is_inconsistently_moved = std::nullopt;
    is_inconsistently_partially_moved = std::nullopt;
    is_inconsistently_pinned = std::nullopt;
}


auto spp::analyse::utils::mem_utils::MemoryInfo::remove_partial_move(
    asts::Ast const &ast,
    scopes::Scope *scope)
    -> void {
    genex::actions::remove(ast_partial_moves, &ast);
    if (not ast_partial_moves.empty()) {
        initialized_by(ast, scope);
    }
}


auto spp::analyse::utils::mem_utils::MemoryInfo::snapshot() const
    -> MemoryInfoSnapshot {
    // Create and return the snapshot.
    return MemoryInfoSnapshot(
        std::get<0>(ast_initialization), std::get<0>(ast_moved), ast_partial_moves, ast_pins, ast_linked_pins,
        initialization_counter);
}


auto spp::analyse::utils::mem_utils::MemoryInfo::clone() const
    -> std::unique_ptr<MemoryInfo> {
    auto out = std::make_unique<MemoryInfo>();
    out->ast_initialization = ast_initialization;
    out->ast_moved = ast_moved;
    out->ast_initialization_origin = ast_initialization_origin;
    out->ast_borrowed = ast_borrowed;
    out->ast_partial_moves = ast_partial_moves;
    out->ast_pins = ast_pins;
    out->ast_comptime = ast_clone(ast_comptime);
    out->initialization_counter = initialization_counter;
    out->is_inconsistently_initialized = is_inconsistently_initialized;
    out->is_inconsistently_moved = is_inconsistently_moved;
    out->is_inconsistently_partially_moved = is_inconsistently_partially_moved;
    out->is_inconsistently_pinned = is_inconsistently_pinned;
    return out;
}


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
    if (auto const *arr_literal = asts::ast_cast<asts::ArrayLiteralRepeatedElementAst>(&value_ast); arr_literal != nullptr) {
        const auto x = arr_literal->elem.get();
        validate_symbol_memory(*x, move_ast, sm, true, true, true, true, true, mark_moves, meta);
        return;
    }
    if (auto const *arr_literal = asts::ast_cast<asts::ArrayLiteralExplicitElementsAst>(&value_ast); arr_literal != nullptr) {
        arr_literal->elems
            | genex::views::ptr
            | genex::views::for_each([&](auto *x) { validate_symbol_memory(*x, move_ast, sm, true, true, true, true, true, mark_moves, meta); });
        return;
    }
    if (auto const *tup_literal = asts::ast_cast<asts::TupleLiteralAst>(&value_ast); tup_literal != nullptr) {
        tup_literal->elems
            | genex::views::ptr
            | genex::views::for_each([&](auto *x) { validate_symbol_memory(*x, move_ast, sm, true, true, true, true, true, mark_moves, meta); });
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
        errors::SemanticErrorBuilder<errors::SppInconsistentlyInitializedMemoryUseError>().with_args(
            value_ast, *std::get<0>(pair), *std::get<1>(pair), "moved").with_scopes({sm.current_scope}).raise();
    }

    // Check for inconsistent memory initialization (from branching).
    if (check_move and var_sym->memory_info->is_inconsistently_initialized.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_initialized;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyInitializedMemoryUseError>().with_args(
            value_ast, *std::get<0>(pair), *std::get<1>(pair), "initialized").with_scopes({sm.current_scope}).raise();
    }

    // Check for inconsistent partial memory moving (from branching).
    if (check_move and var_sym->memory_info->is_inconsistently_partially_moved.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_partially_moved;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyInitializedMemoryUseError>().with_args(
            value_ast, *std::get<0>(pair), *std::get<1>(pair), "partially moved").with_scopes({sm.current_scope}).raise();
    }

    // Check for inconsistent pinned memory (from branching).
    if (check_pins and var_sym->memory_info->is_inconsistently_pinned.has_value()) {
        const auto pair = *var_sym->memory_info->is_inconsistently_pinned;
        errors::SemanticErrorBuilder<errors::SppInconsistentlyPinnedMemoryUseError>().with_args(
            value_ast, *std::get<0>(pair), *std::get<1>(pair)).with_scopes({sm.current_scope}).raise();
    }

    // Check the symbol hasn't already been moved.
    if (check_move and std::get<0>(var_sym->memory_info->ast_moved) != nullptr) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto [where_moved, _] = var_sym->memory_info->ast_moved;
        errors::SemanticErrorBuilder<errors::SppUninitializedMemoryUseError>().with_args(
            value_ast, *where_init, *where_moved).with_scopes({sm.current_scope}).raise();
    }

    // Check the symbol doesn't have any outstanding partial moved (moving a partially moved object).
    if (check_partial_move and not var_sym->memory_info->ast_partial_moves.empty() and asts::ast_cast<asts::IdentifierAst>(&value_ast) != nullptr) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto where_pm = var_sym->memory_info->ast_partial_moves.front();
        errors::SemanticErrorBuilder<errors::SppPartiallyInitializedMemoryUseError>().with_args(
            value_ast, *where_init, *where_pm).with_scopes({sm.current_scope}).raise();
    }

    // Check the symbol doesn't have any outstanding partial moves (directly moving a partial move).
    if (check_partial_move and not var_sym->memory_info->ast_partial_moves.empty() and asts::ast_cast<asts::IdentifierAst>(&value_ast) == nullptr) {
        const auto overlaps = var_sym->memory_info->ast_partial_moves
            | genex::views::filter([&value_ast](auto const &x) { return memory_region_right_overlap(*x, value_ast); })
            | genex::to<std::vector>();
        if (not overlaps.empty()) {
            const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
            const auto where_pm = overlaps.front();
            errors::SemanticErrorBuilder<errors::SppUninitializedMemoryUseError>().with_args(
                value_ast, *where_init, *where_pm).with_scopes({sm.current_scope}).raise();
        }
    }

    // Check the symbol isn't being moved from a borrowed context.
    if (check_move_from_borrowed_ctx and std::get<0>(var_sym->memory_info->ast_borrowed) and asts::ast_cast<asts::IdentifierAst>(&value_ast) == nullptr and not partial_copies) {
        const auto [where_borrow, _] = var_sym->memory_info->ast_borrowed;
        const auto [where_pm, _] = var_sym->memory_info->ast_borrowed;
        errors::SemanticErrorBuilder<errors::SppMoveFromBorrowedMemoryError>().with_args(
            value_ast, *where_pm, *where_borrow).with_scopes({sm.current_scope}).raise();
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
        errors::SemanticErrorBuilder<errors::SppMoveFromPinnedMemoryError>().with_args(
            value_ast, *where_init, *where_move, *where_pin).with_scopes({sm.current_scope}).raise();
    }

    if (check_linked_pins and not symbolic_pin_links.empty()) {
        const auto [where_init, _] = var_sym->memory_info->ast_initialization_origin;
        const auto where_move = &move_ast;
        const auto where_pin = symbolic_pin_links.front().get();
        errors::SemanticErrorBuilder<errors::SppMoveFromPinLinkedMemoryError>().with_args(
            value_ast, *where_init, *where_move, *where_pin).with_scopes({sm.current_scope}).raise();
    }

    // Mark the symbol as moved/partially-moved if it is not copyable.
    if (mark_moves and asts::ast_cast<asts::IdentifierAst>(&value_ast) != nullptr and not copies) {
        var_sym->memory_info->moved_by(value_ast, sm.current_scope);
    }

    else if (mark_moves and asts::ast_cast<asts::IdentifierAst>(&value_ast) == nullptr and not partial_copies) {
        var_sym->memory_info->ast_partial_moves.emplace_back(&value_ast);
    }
}


// template <typename T>
// auto spp::analyse::utils::mem_utils::validate_inconsistent_memory(
//     std::vector<T> const &branches,
//     scopes::ScopeManager *sm,
//     asts::meta::CompilerMetaData *meta)
//     -> void {
//     // Define a simple alias for a list of symbols and their memory.
//     using SymbolMemoryList = std::vector<std::pair<T, MemoryInfoSnapshot>>;
//
//     auto sym_mem_info = std::map<scopes::VariableSymbol*, SymbolMemoryList>();
//     for (auto &&branch : branches) {
//         // Make a record of the symbols' memory status in the scope before the branch is analysed.
//         auto var_symbols_in_scope = sm->current_scope->all_var_symbols()
//             | genex::to<std::vector>();
//
//         auto old_symbol_mem_info = var_symbols_in_scope
//             | genex::views::transform([](auto const &x) { return std::make_pair(x.get(), x->memory_info->snapshot()); })
//             | genex::to<std::vector>();
//
//         // Analyse the memory and then recheck the symbols' memory status.
//         branch->stage_8_check_memory(sm, meta);
//         auto new_symbol_mem_info = var_symbols_in_scope
//             | genex::views::transform([](auto const &x) { return std::make_pair(x.get(), x->memory_info->snapshot()); })
//             | genex::to<std::vector>();
//
//         // Reset the memory status of the symbols for the next branch to analyse with the same original memory states.
//         for (auto &&[sym, old_mem_status] : old_symbol_mem_info) {
//             sym->memory_info->ast_initialization = {old_mem_status.ast_initialization, std::get<1>(sym->memory_info->ast_initialization)};
//             sym->memory_info->ast_moved = {old_mem_status.ast_moved, std::get<1>(sym->memory_info->ast_moved)};
//             sym->memory_info->ast_partial_moves = old_mem_status.ast_partial_moves;
//             sym->memory_info->ast_pins = old_mem_status.ast_pins;
//             sym->memory_info->initialization_counter = old_mem_status.initialization_counter;
//
//             // Save this memory status for subsequent inter-branch status comparisons.
//             auto new_symbol_mem_info_map = std::map(new_symbol_mem_info.begin(), new_symbol_mem_info.end());
//             sym_mem_info[sym].emplace_back(branch, new_symbol_mem_info_map[sym]);
//         }
//     }
//
//     // Check for consistency among the branches' symbols' memory states.
//     for (auto &&[sym, branches_memory_info_lists] : sym_mem_info) {
//         auto first_branch = branches.front();
//         auto first_branch_mem_info = branches_memory_info_lists.at(0).second;
//
//         // Assuming all new memory states are consistent across branches, update o the first "new" state list.
//         sym->memory_info->ast_initialization = {first_branch_mem_info.ast_initialization, std::get<1>(sym->memory_info->ast_initialization)};
//         sym->memory_info->ast_moved = {first_branch_mem_info.ast_moved, std::get<1>(sym->memory_info->ast_moved)};
//         sym->memory_info->ast_partial_moves = first_branch_mem_info.ast_partial_moves;
//         sym->memory_info->ast_pins = first_branch_mem_info.ast_pins;
//         sym->memory_info->initialization_counter = first_branch_mem_info.initialization_counter;
//
//         // Check the new memory status for each symbol is consistent across all branches.
//         for (auto &&[branch, branch_memory_info_list] : branches_memory_info_lists) {
//             // Check for consistent initialization.
//             if ((first_branch_mem_info.ast_initialization == nullptr) != (branch_memory_info_list.ast_initialization == nullptr)) {
//                 sym->memory_info->is_inconsistently_initialized = std::make_pair(first_branch, branch);
//             }
//
//             // Check for consistent moved state.
//             if ((first_branch_mem_info.ast_moved == nullptr) != (branch_memory_info_list.ast_moved == nullptr)) {
//                 sym->memory_info->is_inconsistently_moved = std::make_pair(first_branch, branch);
//             }
//
//             // Check for consistent partial moves.
//             if (first_branch_mem_info.ast_partial_moves != branch_memory_info_list.ast_partial_moves) {
//                 sym->memory_info->is_inconsistently_partially_moved = std::make_pair(first_branch, branch);
//             }
//
//             // Check for consistent pins.
//             if (first_branch_mem_info.ast_pins != branch_memory_info_list.ast_pins) {
//                 sym->memory_info->is_inconsistently_pinned = std::make_pair(first_branch, branch);
//             }
//         }
//     }
// }
//
//
// template auto spp::analyse::utils::mem_utils::validate_inconsistent_memory(
//     std::vector<asts::CaseExpressionBranchAst*> const &branches,
//     scopes::ScopeManager *sm,
//     asts::meta::CompilerMetaData *meta)
//     -> void;
//
//
// template auto spp::analyse::utils::mem_utils::validate_inconsistent_memory(
//     std::vector<asts::IterExpressionBranchAst*> const &branches,
//     scopes::ScopeManager *sm,
//     asts::meta::CompilerMetaData *meta)
//     -> void;
