module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.mem_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import genex;
import std;

namespace spp::analyse::utils::mem_utils {
  namespace {
    /**
     * Compare two escaping-borrow container lists by the memory regions they name, rather than by ast identity. Each
     * branch of a "case" builds its own ast nodes, so the same borrow written in two branches is two pointers but one
     * region, and only the region is what makes the branches agree or disagree.
     */
    auto EscapingBorrowContainersDiffer(
      Vec<spp::Tup<asts::Ast const*, asts::Ast const*>> const &lhs,
      Vec<spp::Tup<asts::Ast const*, asts::Ast const*>> const &rhs)
      -> bool {
      const auto regions = [](auto const &list) {
        auto out = Vec<spp::Str>();
        for (auto const &[container, borrow] : list) {
          out.EmplaceBack(container->ToString() + " <- " + borrow->ToString());
        }
        genex::actions::sort(out);
        return out;
      };
      return regions(lhs) != regions(rhs);
    }

    /**
     * Raise if any borrow a value carries would out-live what it borrows from, once that value is held by @p lhs .
     *
     * @n
     * A coroutine handle keeps the borrows its call was given alive for as long as the handle lives, so putting one
     * into a symbol declared further out moves those borrows past the frame that owns what they point at. Each is
     * checked on its own: one borrow out-living its source is enough, however many the handle carries.
     *
     * @param escaping_borrows The borrows the value carries.
     * @param lhs The symbol the value is being put into.
     * @param owner The ast to report the error against.
     * @param sm The scope manager, for resolving each borrow's source.
     */
    auto EnforceEscapingBorrowsOutlive(
      Vec<spp::Tup<asts::Ast const*, bool, scopes::Scope*>> const &escaping_borrows,
      scopes::VariableSymbol const &lhs,
      asts::Ast *owner,
      scopes::ScopeManager const &sm)
      -> void {
      //
      namespace errors = spp::analyse::errors;
      const auto lhs_init_scope = lhs.ScopeDefinedIn;
      if (lhs_init_scope == nullptr) { return; }

      for (auto const &[e, _, _] : escaping_borrows) {
        const auto source_sym = sm.CurrentScope->GetVarSymbolOutermost(*e).first;
        if (source_sym == nullptr or source_sym->ScopeDefinedIn == nullptr) { continue; }

        // The source out-lives the destination exactly when its scope is one the destination sits inside of, which is
        // what finding it among the destination's ancestors says.
        const auto found_at = genex::position(
          lhs_init_scope->Ancestors(), genex::operations::eq_fixed{source_sym->ScopeDefinedIn});
        spp::RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
          found_at < 0, {sm.CurrentScope}, ERR_ARGS(*owner, *lhs.Name, *e));
      }
    }

    /**
     * This function is another, slightly more relaxed memory region overlap check. It does the same as
     * @ref memory_region_overlap, but only checks one way. This means that @c {a R_OVERLAP a.b} will result in a
     * positive match, but @c {a.b R_OVERLAP a.b} will not.
     * @param ast_1 The lhs AST to check for overlap.
     * @param ast_2 The rhs AST to check for overlap.
     * @return Whether the two memory regions overlap in the right direction.
     */
    auto MemRegionRightOverlap(
      asts::Ast const &ast_1,
      asts::Ast const &ast_2) -> bool {
      const auto s1 = ast_1.ToString();
      const auto s2 = ast_2.ToString();
      return s2.starts_with(s1);
    }
  }
}

auto spp::analyse::utils::mem_utils::MemRegionOverlap(
  asts::Ast const &ast_1,
  asts::Ast const &ast_2)
  -> bool {
  const auto s1 = ast_1.ToString();
  const auto s2 = ast_2.ToString();
  return s1.starts_with(s2) or s2.starts_with(s1);
}

auto spp::analyse::utils::mem_utils::ValidateSymbolMemory(
  asts::ExpressionAst &value_ast,
  asts::Ast const &move_ast,
  scopes::ScopeManager &sm,
  const bool check_move,
  const bool check_partial_move,
  const bool check_move_from_borrowed_ctx,
  const bool mark_moves,
  asts::meta::CompilerMetaData *meta,
  const bool check_escaping_borrow_move) -> void {
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
  const auto copies = var_scope->GetTypeSymbol(var_sym->Type.get())->IsCopyable();
  const auto partial_copies = var_scope->GetTypeSymbol(value_ast.InferType(&sm, meta).get())->IsCopyable();

  // A move only actually occurs when the accessed value is non-copyable.
  const auto moves_value = value_ast.To<asts::IdentifierAst>() != nullptr ? not copies : not partial_copies;

  // Check for inconsistent memory initialization (from branching).
  if (var_sym->MemInfo->IsInconsistentlyInitialized.has_value()) {
    const auto pair = *var_sym->MemInfo->IsInconsistentlyInitialized;
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.first, *pair.second, "initialized"));
  }

  // Check for inconsistent memory moving (from branching).
  if (var_sym->MemInfo->IsInconsistentlyMoved.has_value()) {
    const auto pair = *var_sym->MemInfo->IsInconsistentlyMoved;
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.first, *pair.second, "moved"));
  }

  // Check for inconsistent partial memory moving (from branching).
  if (var_sym->MemInfo->IsInconsistentlyPartiallyMoved.has_value()) {
    const auto pair = *var_sym->MemInfo->IsInconsistentlyPartiallyMoved;
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.first, *pair.second, "partially moved"));
  }

  // Check for inconsistent escaping borrows (from branching).
  if (var_sym->MemInfo->IsInconsistentlyBorrowEscaping.has_value()) {
    const auto pair = *var_sym->MemInfo->IsInconsistentlyBorrowEscaping;
    Raise<errors::SppInconsistentlyEscapingBorrows>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.first, *pair.second));
  }

  // Check the symbol hasn't already been moved.
  if (spp::get<0>(var_sym->MemInfo->AstMoved) != nullptr) {
    const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
    const auto [where_moved, _] = var_sym->MemInfo->AstMoved;
    Raise<errors::SppUninitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_moved));
  }

  // Check we aren't trying to move an escaping borrow
  // (unless copyable).
  if (check_move and moves_value and not var_sym->MemInfo->AstContainersOfEscapingBorrows.IsEmpty()) {
    const auto [where_contained, _] = var_sym->MemInfo->AstContainersOfEscapingBorrows[0];
    Raise<errors::SppMovingEscapingBorrowedMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(*where_contained, move_ast));
  }

  // Check we aren't trying to move the container of an
  // escaping borrow (unless copyable). The container keeps
  // the borrows alive, so moving it propagates them out of
  // the frame that owns the borrowed values.
  if (check_escaping_borrow_move and check_move and moves_value
    and not var_sym->MemInfo->AstContainedEscapingBorrows.IsEmpty()) {
    Raise<errors::SppMovingEscapingBorrowedMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(*var_sym->Name, move_ast));
  }

  // Check we aren't trying to move a comptime constant
  // (unless copyable).
  if (check_move and moves_value and var_sym->MemInfo->AstCompTime != nullptr) {
    Raise<errors::SppMovingComptimeConstantMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, move_ast));
  }

  // Check the symbol doesn't have any outstanding partial
  // moved (moving a partially moved object).
  if (check_partial_move and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<asts::IdentifierAst>() !=
    nullptr) {
    const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
    const auto where_pm = var_sym->MemInfo->AstPartialMoves.Front();
    Raise<errors::SppPartiallyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
  }

  // Check the symbol doesn't have any outstanding partial
  // moves (directly moving a partial move).
  if (check_partial_move and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<asts::IdentifierAst>() ==
    nullptr) {
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

  // Check the symbol isn't being moved from a borrowed
  // context.
  if (check_move_from_borrowed_ctx and spp::get<0>(var_sym->MemInfo->AstBorrowed) and value_ast.To<
    asts::IdentifierAst>() == nullptr and not partial_copies) {
    const auto [where_borrow, _] = var_sym->MemInfo->AstBorrowed;
    const auto [where_pm, _] = var_sym->MemInfo->AstBorrowed;
    Raise<errors::SppMoveFromBorrowedMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_pm, *where_borrow));
  }

  // A narrowed view of a value is that value: consuming
  // "x" inside "case x is Some[T](..)" discharges what
  // "x" named, not merely the branch-local symbol standing
  // for it. Walked up the chain, since a pattern can narrow
  // what an enclosing pattern already narrowed.
  const auto mark_chain = [](scopes::VariableSymbol *sym, auto &&mark) {
    for (auto *s = sym; s != nullptr; s = s->NarrowsSym.get()) { mark(s); }
  };

  // Mark the symbol as moved/partially-moved if it is not
  // copyable.
  if (mark_moves and value_ast.To<asts::IdentifierAst>() != nullptr and not copies) {
    mark_chain(var_sym, [&](scopes::VariableSymbol *s) { s->MemInfo->MovedBy(value_ast, sm.CurrentScope); });
  }

  // Only whole moves carry up the chain. A pattern binding
  // an attribute out of the narrowed view partially moves
  // that view, but the value it narrows is still whole as
  // far as anything outside the branch is concerned - and
  // marking it otherwise makes the subject itself unreadable
  // at the "case" that introduced the narrowing.
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
  // Define a simple alias for a list of symbols and their
  // memory.
  using SymbolMemoryList = Vec<Pair<asts::CaseExpressionBranchAst*, mem_info_utils::MemoryInfoSnapshot>>;
  using SymbolMemoryMap = Map<scopes::VariableSymbol*, mem_info_utils::MemoryInfoSnapshot>;

  // Create a map of the symbols' memory  information before
  // any branches are analysed.
  auto sym_mem_info = std::map<scopes::VariableSymbol*, SymbolMemoryList>();

  // The lookup walks ancestors and super scopes, which can
  // reach one symbol by more than one route, and every list
  // below is built with one entry per branch per occurrence.
  // Deduplicate.
  auto vs = Vec<scopes::VariableSymbol*>();
  auto seen_syms = Set<scopes::VariableSymbol*>();
  for (auto *sym : sm->CurrentScope->AllVarSymbols()) {
    if (seen_syms.insert(sym).second) { vs.EmplaceBack(sym); }
  }

  auto pre_analysis_mem_info = vs
    | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
    | genex::to<Vec>();

  // Make a record of the symbols' memory status in the scope
  // before the branch is analysed.
  auto old_symbol_mem_info = vs
    | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
    | genex::to<Vec>();

  for (auto &&branch : branches) {
    // Analyse the memory and then recheck the symbols' memory
    // status.
    branch->Stage8_CheckMemory(sm, meta);
    auto new_symbol_mem_info = vs
      | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
      | genex::to<Vec>();

    // Reset the memory status of the symbols for the next branch
    // to analyse with the same original memory states.
    // Todo: Scopes need restoring properly too. (And rename to AstInit + Reformat).
    // Built once per branch rather than once per symbol: it is the same map every time round, and rebuilding it
    // inside the loop made recording one branch's states quadratic in the number of symbols in scope.
    auto new_symbol_mem_info_map = SymbolMemoryMap(new_symbol_mem_info.begin(), new_symbol_mem_info.end());

    for (auto &&[sym, old_mem_status] : old_symbol_mem_info) {
      sym->MemInfo->AstInitialization = {
        old_mem_status.AstInitialization,
        spp::get<1>(sym->MemInfo->AstInitialization)
      };
      sym->MemInfo->AstMoved = {old_mem_status.AstMoved, spp::get<1>(sym->MemInfo->AstMoved)};
      sym->MemInfo->AstPartialMoves = old_mem_status.AstPartialMoves;
      sym->MemInfo->AstContainedEscapingBorrows = old_mem_status.AstContainedEscapingBorrows;
      sym->MemInfo->AstContainersOfEscapingBorrows = old_mem_status.AstContainersOfEscapingBorrows;
      sym->MemInfo->InitializationCounter = old_mem_status.InitializationCounter;

      // Save this memory status for subsequent inter-branch
      // status comparisons.
      sym_mem_info[sym].EmplaceBack(branch, new_symbol_mem_info_map[sym]);
    }
  }

  // Add the pre-analysis memory states as a "final" branch
  // (just for comparison purposes).
  for (auto &&[sym, mem_info_list] : pre_analysis_mem_info) {
    sym_mem_info[sym].EmplaceBack(nullptr, std::move(mem_info_list));
  }

  // Get the first "non-terminating" branch, and update the
  // symbols to reflect its memory state.
  const auto non_terminating_branch = genex::find_if(
    branches, [](auto const &x) { return not x->Body->Terminates(); });
  const auto first_branch = non_terminating_branch == branches.end() ? parent : *non_terminating_branch;
  const auto first_branch_index = non_terminating_branch != branches.end()
    ? genex::iterators::distance(branches.begin(), non_terminating_branch)
    : -1;
  const auto first_branch_mem_info_getter = [&](auto const &branch_mem_info) {
    return first_branch_index != -1
      ? branch_mem_info.At(static_cast<std::size_t>(first_branch_index)).second
      : branch_mem_info.Back().second;
  };

  const auto has_else_branch = not branches.IsEmpty()
    ? branches.Back()->Patterns[0]->To<asts::CasePatternVariantElseAst>()
    : nullptr;
  const auto skip_else = has_else_branch and has_else_branch->MarkedForIterLoopExit();

  // Check for consistency among the branches' symbols' memory
  // states.
  for (auto const &[sym, branches_memory_info_lists] : sym_mem_info) {
    auto first_branch_mem_info = first_branch_mem_info_getter(branches_memory_info_lists);

    // Assuming all new memory states are consistent across
    // branches, update to the first "new" state list.
    sym->MemInfo->AstInitialization = {
      first_branch_mem_info.AstInitialization, spp::get<1>(sym->MemInfo->AstInitialization)
    };
    sym->MemInfo->AstMoved = {first_branch_mem_info.AstMoved, spp::get<1>(sym->MemInfo->AstMoved)};
    sym->MemInfo->AstPartialMoves = first_branch_mem_info.AstPartialMoves;
    sym->MemInfo->AstContainedEscapingBorrows = first_branch_mem_info.AstContainedEscapingBorrows;
    sym->MemInfo->AstContainersOfEscapingBorrows = first_branch_mem_info.AstContainersOfEscapingBorrows;
    sym->MemInfo->InitializationCounter = first_branch_mem_info.InitializationCounter;

    // Check the new memory status for each symbol is
    // consistent across all branches that don't terminate.
    auto applicable_branch_memory_info_lists = branches_memory_info_lists
      | genex::views::remove_if([&](auto const &x) {
        return x.first == nullptr or x.first->Body->Terminates()
          or (skip_else and not branches.IsEmpty() and x.first == branches.Back());
      })
      | genex::to<Vec>();

    for (auto const &[branch, branch_memory_info_list] : applicable_branch_memory_info_lists) {
      // Check for consistent initialization.
      if ((first_branch_mem_info.AstInitialization == nullptr) != (branch_memory_info_list.AstInitialization ==
        nullptr)) {
        sym->MemInfo->IsInconsistentlyInitialized = {first_branch, branch};
      }

      // Check for consistent moved state.
      if ((first_branch_mem_info.AstMoved == nullptr) != (branch_memory_info_list.AstMoved == nullptr)) {
        sym->MemInfo->IsInconsistentlyMoved = {first_branch, branch};
      }

      // Check for consistent partial moves.
      if (first_branch_mem_info.AstPartialMoves != branch_memory_info_list.AstPartialMoves) {
        sym->MemInfo->IsInconsistentlyPartiallyMoved = {first_branch, branch};
      }

      // Check for consistent escaping borrows, from both ends
      // of the link: a symbol can be the coroutine handle that
      // holds the borrows, or the owner of the memory they
      // borrow, and only the second is what a later use of that
      // memory (eg moving it) is checked against.
      if (first_branch_mem_info.AstContainedEscapingBorrows != branch_memory_info_list.AstContainedEscapingBorrows
        or EscapingBorrowContainersDiffer(
          first_branch_mem_info.AstContainersOfEscapingBorrows,
          branch_memory_info_list.AstContainersOfEscapingBorrows)) {
        sym->MemInfo->IsInconsistentlyBorrowEscaping = {first_branch, branch};
      }
    }
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

  // Prevent a borrow being placed into a value with a longer
  // lifetime.
  const auto is_rhs_borrow = override_borrow or (
    rhs_outermost and spp::get<0>(rhs_outermost->MemInfo->AstBorrowed) != nullptr);
  if (lhs_outermost != nullptr and rhs_outermost != nullptr and is_rhs_borrow) {
    const auto rhs_borrow_scope = spp::get<1>(rhs_outermost->MemInfo->AstBorrowed) ? : sm.CurrentScope;
    const auto lhs_init_scope = lhs_outermost->ScopeDefinedIn;
    if (lhs_init_scope != nullptr) {
      const auto scope_depth_difference = genex::position(
        lhs_init_scope->Ancestors(), genex::operations::eq_fixed{rhs_borrow_scope});
      RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
        scope_depth_difference < 0, {sm.CurrentScope},
        ERR_ARGS(*owner, *lhs_outermost->Name, *(spp::get<0>(rhs_outermost->MemInfo->AstBorrowed) ?: &rhs_expr)));
    }
  }

  // Ensure a value that contains escaping borrows isn't
  // increasing the escaping borrows' lifetimes.
  else if (lhs_outermost != nullptr and rhs_outermost != nullptr) {
    EnforceEscapingBorrowsOutlive(
      rhs_outermost->MemInfo->AstContainedEscapingBorrows, *lhs_outermost, owner, sm);
  }

  // The same, for a right-hand side that names no symbol of
  // its own. A call written straight into the destination
  // ("x = c(&s)") has nowhere to record what it carries but
  // the destination itself, so the borrows are read back off
  // there rather than off a handle the source never bound.
  else if (lhs_outermost != nullptr and rhs_expr.To<asts::PostfixExpressionAst>() != nullptr) {
    EnforceEscapingBorrowsOutlive(
      lhs_outermost->MemInfo->AstContainedEscapingBorrows, *lhs_outermost, owner, sm);
  }

  // Ensure a value that contains escaping borrows isn't
  // increasing the escaping borrows' lifetimes for "gen.res()"
  // As the borrow is a temporary (no scope), the topmost
  // branch uses "current scope".
  else if (const auto pf = rhs_expr.To<asts::PostfixExpressionAst>(); pf and pf->Op->To<
    asts::PostfixExpressionOperatorKeywordResAst>()) {
    const auto new_rhs_sym = sm.CurrentScope->GetVarSymbolOutermost(*pf->Lhs).first;
    PreventBorrowLifetimeExtension(*pf->Lhs, lhs_outermost, new_rhs_sym, owner, sm, true);
  }
}
