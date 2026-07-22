module;
#include <spp/analyse/macros.hpp>

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
import spp.asts.convention_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import ankerl;
import genex;

auto spp::analyse::utils::mem_utils::MemRegionOverlap(
    asts::Ast const &ast_1,
    asts::Ast const &ast_2)
    -> bool {
    const auto s1 = ast_1.ToString();
    const auto s2 = ast_2.ToString();
    return s1.starts_with(s2) or s2.starts_with(s1);
}

auto spp::analyse::utils::mem_utils::MemRegionRightOverlap(
    asts::Ast const &ast_1,
    asts::Ast const &ast_2) -> bool {
    const auto s1 = ast_1.ToString();
    const auto s2 = ast_2.ToString();
    return s2.starts_with(s1);
}

auto spp::analyse::utils::mem_utils::ValidateSymbolMemory(
    asts::ExpressionAst &value_ast,
    asts::Ast const &move_ast,
    scopes::ScopeManager &sm,
    const bool check_move,
    const bool check_partial_move,
    const bool check_move_from_borrowed_ctx,
    const bool mark_moves,
    asts::meta::CompilerMetaData *meta) -> void {
    // For tuple and array literals, recursively analyse each element.
    if (auto const *arr_literal = value_ast.To<asts::ArrayLiteralRepeatedElementAst>(); arr_literal != nullptr) {
        const auto x = arr_literal->Elem.get();
        ValidateSymbolMemory(*x, move_ast, sm, true, true, true, mark_moves, meta);
        return;
    }
    if (auto const *arr_literal = value_ast.To<asts::ArrayLiteralExplicitElementsAst>(); arr_literal != nullptr) {
        for (auto &&x : arr_literal->Elems) {
            ValidateSymbolMemory(*x, move_ast, sm, true, true, true, mark_moves, meta);
        }
        return;
    }
    if (auto const *tup_literal = value_ast.To<asts::TupleLiteralAst>(); tup_literal != nullptr) {
        for (auto &&x : tup_literal->Elems) {
            ValidateSymbolMemory(*x, move_ast, sm, true, true, true, mark_moves, meta);
        }
        return;
    }

    // Get the symbol representing the outermost part of the expression being moved. Non-symbolic => temporary value.
    auto [var_sym, var_scope] = sm.CurrentScope->GetVarSymbolOutermost(value_ast);
    if (var_sym == nullptr) { return; }
    const auto copies = var_scope->GetTypeSymbol(var_sym->Type)->IsCopyable();
    const auto partial_copies = var_scope->GetTypeSymbol(value_ast.InferType(&sm, meta))->IsCopyable();

    // A move only actually occurs when the accessed value is non-copyable.
    const auto moves_value = value_ast.To<asts::IdentifierAst>() != nullptr ? not copies : not partial_copies;

    // Check for inconsistent memory initialization (from branching).
    if (var_sym->MemInfo->IsInconsistentlyInitialized.has_value()) {
        const auto pair = *var_sym->MemInfo->IsInconsistentlyInitialized;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.First, *pair.Second, "initialized"));
    }

    // Check for inconsistent memory moving (from branching).
    if (var_sym->MemInfo->IsInconsistentlyMoved.has_value()) {
        const auto pair = *var_sym->MemInfo->IsInconsistentlyMoved;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.First, *pair.Second, "moved"));
    }

    // Check for inconsistent partial memory moving (from branching).
    if (var_sym->MemInfo->IsInconsistentlyPartiallyMoved.has_value()) {
        const auto pair = *var_sym->MemInfo->IsInconsistentlyPartiallyMoved;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.First, *pair.Second, "partially moved"));
    }

    // Check for inconsistent escaping borrows (from branching).
    if (var_sym->MemInfo->IsInconsistentlyBorrowEscaping.has_value()) {
        const auto pair = *var_sym->MemInfo->IsInconsistentlyBorrowEscaping;
        Raise<errors::SppInconsistentlyEscapingBorrows>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.First, *pair.Second));
    }

    // Check the symbol hasn't already been moved.
    if (std::get<0>(var_sym->MemInfo->AstMoved) != nullptr) {
        const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
        const auto [where_moved, _] = var_sym->MemInfo->AstMoved;
        Raise<errors::SppUninitializedMemoryUseError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_moved));
    }

    // Check we aren't trying to move an escaping borrow (unless copyable).
    if (check_move and moves_value and not var_sym->MemInfo->AstContainersOfEscapingBorrows.IsEmpty()) {
        const auto [where_contained, _] = var_sym->MemInfo->AstContainersOfEscapingBorrows[0];
        Raise<errors::SppMovingEscapingBorrowedMemoryError>(
            {sm.CurrentScope}, ERR_ARGS(*where_contained, move_ast));
    }

    // Check we aren't trying to move a comptime constant (unless copyable).
    if (check_move and moves_value and var_sym->MemInfo->AstCompTime != nullptr) {
        Raise<errors::SppMovingComptimeConstantMemoryError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, move_ast));
    }

    // Check the symbol doesn't have any outstanding partial moved (moving a partially moved object).
    if (check_partial_move and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<asts::IdentifierAst>() != nullptr) {
        const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
        const auto where_pm = var_sym->MemInfo->AstPartialMoves.Front();
        Raise<errors::SppPartiallyInitializedMemoryUseError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
    }

    // Check the symbol doesn't have any outstanding partial moves (directly moving a partial move).
    if (check_partial_move and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<asts::IdentifierAst>() == nullptr) {
        const auto overlaps = var_sym->MemInfo->AstPartialMoves
            | genex::views::filter([&](auto const &x) { return MemRegionRightOverlap(*x, value_ast); })
            | genex::to<Vec>();
        if (not overlaps.IsEmpty()) {
            const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
            const auto where_pm = overlaps.Front();
            Raise<errors::SppUninitializedMemoryUseError>(
                {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
        }
    }

    // Check the symbol isn't being moved from a borrowed context.
    if (check_move_from_borrowed_ctx and std::get<0>(var_sym->MemInfo->AstBorrowed) and value_ast.To<asts::IdentifierAst>() == nullptr and not partial_copies) {
        const auto [where_borrow, _] = var_sym->MemInfo->AstBorrowed;
        const auto [where_pm, _] = var_sym->MemInfo->AstBorrowed;
        Raise<errors::SppMoveFromBorrowedMemoryError>(
            {sm.CurrentScope}, ERR_ARGS(value_ast, *where_pm, *where_borrow));
    }

    // Mark the symbol as moved/partially-moved if it is not copyable.
    if (mark_moves and value_ast.To<asts::IdentifierAst>() != nullptr and not copies) {
        var_sym->MemInfo->MovedBy(value_ast, sm.CurrentScope);
    }

    else if (mark_moves and value_ast.To<asts::IdentifierAst>() == nullptr and not partial_copies) {
        var_sym->MemInfo->AstPartialMoves.EmplaceBack(&value_ast);
    }
}

auto spp::analyse::utils::mem_utils::ValidateInconsistentMemory(
    asts::Ast *parent,
    Vec<asts::CaseExpressionBranchAst*> const &branches,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Define a simple alias for a list of symbols and their memory.
    using SymbolMemoryList = Vec<Pair<asts::CaseExpressionBranchAst*, mem_info_utils::MemoryInfoSnapshot>>;
    using SymbolMemoryMap = ankerl::unordered_dense::map<scopes::VariableSymbol*, mem_info_utils::MemoryInfoSnapshot>;

    // Create a map of the symbols' memory  information before any branches are analysed.
    auto sym_mem_info = std::map<scopes::VariableSymbol*, SymbolMemoryList>();
    auto vs = sm->CurrentScope->AllVarSymbols();
    auto pre_analysis_mem_info = vs
        | genex::views::transform([](auto const &x) { return MakePair(x.get(), x->MemInfo->Snapshot()); })
        | genex::to<Vec>();

    // Make a record of the symbols' memory status in the scope before the branch is analysed.
    auto old_symbol_mem_info = vs
        | genex::views::transform([](auto const &x) { return MakePair(x.get(), x->MemInfo->Snapshot()); })
        | genex::to<Vec>();

    for (auto &&branch : branches) {
        // Analyse the memory and then recheck the symbols' memory status.
        branch->Stage8_CheckMemory(sm, meta);
        auto new_symbol_mem_info = vs
            | genex::views::transform([](auto const &x) { return MakePair(x.get(), x->MemInfo->Snapshot()); })
            | genex::to<Vec>();

        // Reset the memory status of the symbols for the next branch to analyse with the same original memory states.
        // Todo: Scopes need restoring properly too.
        for (auto &&[sym, old_mem_status] : old_symbol_mem_info) {
            sym->MemInfo->AstInitialization = std::make_tuple(old_mem_status.AstInitialization, std::get<1>(sym->MemInfo->AstInitialization));
            sym->MemInfo->AstMoved = {old_mem_status.AstMoved, std::get<1>(sym->MemInfo->AstMoved)};
            sym->MemInfo->AstPartialMoves = old_mem_status.AstPartialMoves;
            sym->MemInfo->AstContainedEscapingBorrows = old_mem_status.AstContainedEscapingBorrows;
            sym->MemInfo->InitializationCounter = old_mem_status.InitializationCounter;

            // Save this memory status for subsequent inter-branch status comparisons.
            auto new_symbol_mem_info_map = SymbolMemoryMap(new_symbol_mem_info.begin(), new_symbol_mem_info.end());
            sym_mem_info[sym].EmplaceBack(branch, new_symbol_mem_info_map[sym]);
        }
    }

    // Add the pre-analysis memory states as a "final" branch (just for comparison purposes).
    for (auto &&[sym, mem_info_list] : pre_analysis_mem_info) {
        sym_mem_info[sym].EmplaceBack(nullptr, std::move(mem_info_list));
    }

    // Get the first "non-terminating" branch, and update the symbols to reflect its memory state.
    const auto non_terminating_branch = genex::find_if(
        branches, [](auto const &x) { return not x->Body->Terminates(); });
    const auto first_branch = non_terminating_branch == branches.end() ? parent : *non_terminating_branch;
    const auto first_branch_index = non_terminating_branch != branches.end() ? genex::iterators::distance(branches.begin(), non_terminating_branch) : -1;
    const auto first_branch_mem_info_getter = [&](auto const &branch_mem_info) {
        return first_branch_index != -1 ? branch_mem_info.At(static_cast<std::size_t>(first_branch_index)).Second : branch_mem_info.Back().Second;
    };

    const auto has_else_branch = not branches.IsEmpty() ? branches.Back()->Patterns[0]->To<asts::CasePatternVariantElseAst>() : nullptr;
    const auto skip_else = has_else_branch and has_else_branch->MarkedForIterLoopExit();

    // Check for consistency among the branches' symbols' memory states.
    auto current_branch = branches.begin();
    for (auto const &[sym, branches_memory_info_lists] : sym_mem_info) {
        auto first_branch_mem_info = first_branch_mem_info_getter(branches_memory_info_lists);
        if (current_branch == branches.end() - 1 and skip_else) { break; }

        // Assuming all new memory states are consistent across branches, update to the first "new" state list.
        sym->MemInfo->AstInitialization = {first_branch_mem_info.AstInitialization, std::get<1>(sym->MemInfo->AstInitialization)};
        sym->MemInfo->AstMoved = {first_branch_mem_info.AstMoved, std::get<1>(sym->MemInfo->AstMoved)};
        sym->MemInfo->AstPartialMoves = first_branch_mem_info.AstPartialMoves;
        sym->MemInfo->AstContainedEscapingBorrows = first_branch_mem_info.AstContainedEscapingBorrows;
        sym->MemInfo->InitializationCounter = first_branch_mem_info.InitializationCounter;

        // Check the new memory status for each symbol is consistent across all branches that don't terminate.
        auto applicable_branch_memory_info_lists = branches_memory_info_lists
            | genex::views::drop_last(1)
            | genex::views::remove_if([&](auto const &x) { return x.First->Body->Terminates(); })
            | genex::to<Vec>();

        for (auto const &[branch, branch_memory_info_list] : applicable_branch_memory_info_lists) {
            // Check for consistent initialization.
            if ((first_branch_mem_info.AstInitialization == nullptr) != (branch_memory_info_list.AstInitialization == nullptr)) {
                sym->MemInfo->IsInconsistentlyInitialized = MakePair(first_branch, branch);
            }

            // Check for consistent moved state.
            if ((first_branch_mem_info.AstMoved == nullptr) != (branch_memory_info_list.AstMoved == nullptr)) {
                sym->MemInfo->IsInconsistentlyMoved = MakePair(first_branch, branch);
            }

            // Check for consistent partial moves.
            if (first_branch_mem_info.AstPartialMoves != branch_memory_info_list.AstPartialMoves) {
                sym->MemInfo->IsInconsistentlyPartiallyMoved = MakePair(first_branch, branch);
            }

            // Check for consistent escaping borrows.
            if (first_branch_mem_info.AstContainedEscapingBorrows != branch_memory_info_list.AstContainedEscapingBorrows) {
                sym->MemInfo->IsInconsistentlyBorrowEscaping = MakePair(first_branch, branch);
            }
        }

        ++current_branch;
    }
}

auto spp::analyse::utils::mem_utils::PreventBorrowLifetimeExtension(
    asts::Ast const &rhs_expr,
    scopes::VariableSymbol const *lhs_outermost,
    scopes::VariableSymbol const *rhs_outermost,
    asts::Ast *owner,
    scopes::ScopeManager const &sm,
    const bool override_borrow)
    -> void {
    // Todo: A similar version of this function will be needed for "return" statements as-well as the currently used "="
    //  statements.

    // Prevent a borrow being placed into a value with a longer lifetime.
    const auto is_rhs_borrow = override_borrow or (rhs_outermost and std::get<0>(rhs_outermost->MemInfo->AstBorrowed) != nullptr);
    if (lhs_outermost != nullptr and rhs_outermost != nullptr and is_rhs_borrow) {
        const auto rhs_borrow_scope = std::get<1>(rhs_outermost->MemInfo->AstBorrowed) ?: sm.CurrentScope;
        const auto lhs_init_scope = lhs_outermost->ScopeDefinedIn;
        if (lhs_init_scope != nullptr) {
            const auto scope_depth_difference = genex::position(lhs_init_scope->Ancestors(), genex::operations::eq_fixed{rhs_borrow_scope});
            RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
                scope_depth_difference < 0, {sm.CurrentScope},
                ERR_ARGS(*owner, *lhs_outermost->Name, *(std::get<0>(rhs_outermost->MemInfo->AstBorrowed) ?: &rhs_expr)));
        }
    }

    // Ensure a value that contains escaping borrows isn't increasing the escaping borrows' lifetimes.
    else if (lhs_outermost != nullptr and rhs_outermost != nullptr) {
        const auto escaping_borrows = rhs_outermost->MemInfo->AstContainedEscapingBorrows;
        const auto lhs_init_scope = lhs_outermost->ScopeDefinedIn;
        for (auto const &[e, _, _] : escaping_borrows) {
            const auto escaping_borrow_scope = sm.CurrentScope->GetVarSymbolOutermost(*e).First->ScopeDefinedIn;
            if (not escaping_borrow_scope) { continue; }
            const auto scope_depth_difference = genex::position(lhs_init_scope->Ancestors(), genex::operations::eq_fixed{escaping_borrow_scope});
            RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
                scope_depth_difference < 0, {sm.CurrentScope},
                ERR_ARGS(*owner, *lhs_outermost->Name, *e));
        }
    }

    // Ensure a value that contains escaping borrows isn't increasing the escaping borrows' lifetimes for "gen.res()"
    // As the borrow is a temporary (no scope), the topmost branch uses "current scope".
    else if (const auto pf = rhs_expr.To<asts::PostfixExpressionAst>(); pf and pf->Op->To<asts::PostfixExpressionOperatorKeywordResAst>()) {
        const auto new_rhs_sym = sm.CurrentScope->GetVarSymbolOutermost(*pf->Lhs).First.get();
        PreventBorrowLifetimeExtension(*pf->Lhs, lhs_outermost, new_rhs_sym, owner, sm, true);
    }

    // Finally, an inline coroutine call, such as "x = coro(&borrow)" also needs checking.
    // This is also a temporary borrow, but being contained rather than yielded at this point.
    // else if (pf and pf->Op->To<asts::PostfixExpressionOperatorFunctionCallAst>()) {
    //     const auto new_rhs_sym = sm.CurrentScope->GetVarSymbolOutermost(*pf->Lhs).First.get();
    //     const auto lhs_func = pf->Op->To<asts::PostfixExpressionOperatorFunctionCallAst>();
    //     const auto lhs_func_call_is_coro = lhs_func->Target()->IsCoroutine();
    //     const auto has_borrowed_args = lhs_func_call_is_coro and genex::any_of(lhs_func->FnArgGroup->Args, [&](auto const &arg) {
    //         return arg->Conv != nullptr;
    //     });
    //     PreventBorrowLifetimeExtension(*pf->Lhs, lhs_outermost, new_rhs_sym, owner, sm, lhs_func_call_is_coro and has_borrowed_args);
    // }
}
