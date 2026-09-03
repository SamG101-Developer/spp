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
     * Whether @p path names something inside the region @p prefix names, comparing them as paths rather than as raw
     * text.
     *
     * @n
     * A region is reached from another by field access, indexing or a deref, so @c {a} contains @c {a.b} and @c {a[i]}
     * and @c {a@} . What it does not contain is @c {ab} , and a plain @c starts_with says otherwise - which made a
     * variable called @c s report as overlapping one called @c second . The rule is therefore that the prefix has to
     * end where an identifier ends: anything but a further identifier character is a boundary, which keeps every
     * genuine containment while dropping the ones that only share spelling.
     *
     * @todo: probably better to actually travel the member access asts for exact matching?
     * @param prefix The path of the containing region.
     * @param path The path that may sit inside it.
     * @return Whether @p path is @p prefix or something reached from it.
     */
    auto IsRegionPathPrefix(
      Str const &prefix,
      Str const &path) -> bool {
      if (not path.starts_with(prefix)) { return false; }
      if (path.size() == prefix.size()) { return true; }

      const auto c = path[prefix.size()];
      const auto continues_identifier =
        (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_';
      return not continues_identifier;
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
      return IsRegionPathPrefix(ast_1.ToString(), ast_2.ToString());
    }
  }
}

auto spp::analyse::utils::mem_utils::MemRegionOverlap(
  asts::Ast const &ast_1,
  asts::Ast const &ast_2)
  -> bool {
  const auto s1 = ast_1.ToString();
  const auto s2 = ast_2.ToString();
  return IsRegionPathPrefix(s1, s2) or IsRegionPathPrefix(s2, s1);
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
