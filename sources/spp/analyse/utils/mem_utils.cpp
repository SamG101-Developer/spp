module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.mem_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.regions;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.tuple_literal_ast;
import genex;
import std;

namespace spp::analyse::utils::mem_utils {
  namespace {
  }
}

auto spp::analyse::utils::mem_utils::ValidateSymbolMemory(
  ExpressionAst &value_ast,
  Ast const &move_ast,
  ScopeManager &sm,
  const bool check_move,
  const bool check_partial_move,
  const bool check_move_from_borrowed_ctx,
  const bool mark_moves,
  meta::CompilerMetaData *meta,
  const bool check_escaping_borrow_move,
  const bool place_is_written) -> void {
  // For tuple and array literals, recursively analyse each element.
  if (auto const *arr_literal = value_ast.To<ArrayLiteralRepeatedElementAst>(); arr_literal != nullptr) {
    const auto x = arr_literal->Elem.get();
    ValidateSymbolMemory(*x, move_ast, sm, true, true, true, mark_moves, meta);
    return;
  }
  if (auto const *arr_literal = value_ast.To<ArrayLiteralExplicitElementsAst>(); arr_literal != nullptr) {
    for (auto &&x : arr_literal->Elems) {
      ValidateSymbolMemory(*x, move_ast, sm, true, true, true, mark_moves, meta);
    }
    return;
  }
  if (auto const *tup_literal = value_ast.To<TupleLiteralAst>(); tup_literal != nullptr) {
    for (auto &&x : tup_literal->Elems) {
      ValidateSymbolMemory(*x, move_ast, sm, true, true, true, mark_moves, meta);
    }
    return;
  }

  // Get the symbol representing the outermost part of the expression being moved. Non-symbolic => temporary value.
  auto [var_sym, var_scope] = sm.CurrentScope->GetVarSymbolOutermost(value_ast);
  if (var_sym == nullptr) { return; }
  const auto copies = var_sym->TypeRefIn(*var_scope).Sym->IsCopyable();
  const auto partial_copies = var_scope->GetTypeSymbol(value_ast.InferType(&sm, meta).get())->IsCopyable();

  // A move only actually occurs when the accessed value is non-copyable.
  const auto moves_value = value_ast.To<IdentifierAst>() != nullptr ? not copies : not partial_copies;

  // Check for inconsistent memory initialization (from branching).
  if (var_sym->MemInfo->IsInconsistentlyInitialized.has_value()) {
    const auto pair = *var_sym->MemInfo->IsInconsistentlyInitialized;
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.first, *pair.second, "initialized"));
  }

  // Check for inconsistent memory moving (from branching).
  RaiseIfInconsistentlyMoved(*var_sym, value_ast, sm.CurrentScope);

  // Check for inconsistent escaping borrows (from branching).
  if (var_sym->MemInfo->IsInconsistentlyBorrowEscaping.has_value()) {
    const auto pair = *var_sym->MemInfo->IsInconsistentlyBorrowEscaping;
    Raise<errors::SppInconsistentlyEscapingBorrows>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *pair.first, *pair.second));
  }

  // Check the symbol hasn't already been moved.
  RaiseIfMoved(*var_sym, value_ast, sm.CurrentScope);

  // Check we aren't trying to move an escaping borrow
  // (unless copyable).
  if (check_move and moves_value and not var_sym->MemInfo->AstContainersOfEscapingBorrows.IsEmpty()) {
    const auto [where_contained, _] = var_sym->MemInfo->AstContainersOfEscapingBorrows[0];
    Raise<errors::SppMovingEscapingBorrowedMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(*where_contained, move_ast));
  }

  // Copying a value out reads it, which a live "&mut" escaping
  // borrow of it (held by a coroutine, a future or a closure)
  // rules out just as it rules out borrowing it: the holder may
  // be writing to it. Only looked for when something is known
  // to hold an escaping borrow of this symbol.
  if (check_move and not moves_value and not var_sym->MemInfo->AstContainersOfEscapingBorrows.IsEmpty()) {
    for (auto const *holder : sm.CurrentScope->AllVarSymbols()) {
      for (auto const &[borrow, is_mut, _] : holder->MemInfo->AstContainedEscapingBorrows) {
        RaiseIf<errors::SppMemoryOverlapUsageError>(
          is_mut and regions::MemRegionOverlap(*borrow, value_ast),
          {sm.CurrentScope}, ERR_ARGS(*borrow, value_ast));
      }
    }
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
  if (check_move and moves_value and var_sym->IsCompTime()) {
    Raise<errors::SppMovingComptimeConstantMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, move_ast));
  }

  // Check the symbol doesn't have any outstanding partial
  // moved (moving a partially moved object).
  if (check_partial_move and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<IdentifierAst>() !=
    nullptr) {
    const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
    const auto where_pm = var_sym->MemInfo->AstPartialMoves.Front();
    Raise<errors::SppPartiallyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
  }

  // Check the symbol doesn't have any outstanding partial
  // moves (directly moving a partial move).
  if (check_partial_move and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<IdentifierAst>() ==
    nullptr) {
    // "Contains" covers the move naming the place exactly
    // as well as the move naming something the place sits
    // inside of. Writing "o.inner.val" puts the first back
    // but cannot put "o.inner" back, so a write keeps only
    // the second.
    const auto steps = regions::RegionPath(value_ast);
    const auto overlaps = var_sym->MemInfo->AstPartialMoves
      | genex::views::filter([&](auto const &x) {
        const auto path = regions::RegionPath(*x);
        return regions::MemRegionRelate(path, steps) == regions::MemRegionRelation::Contains
          and (not place_is_written or path.Len() < steps.Len());
      })
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
    IdentifierAst>() == nullptr and not partial_copies) {
    const auto [where_borrow, _] = var_sym->MemInfo->AstBorrowed;
    Raise<errors::SppMoveFromBorrowedMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_borrow, *where_borrow));
  }

  // A narrowed view of a value is that value: consuming
  // "x" inside "case x is Some[T](..)" discharges what
  // "x" named, not merely the branch-local symbol standing
  // for it. Walked up the chain, since a pattern can narrow
  // what an enclosing pattern already narrowed.
  const auto mark_chain = [](const auto sym, auto &&mark) {
    for (auto *s = sym; s != nullptr; s = s->NarrowsSym.get()) { mark(s); }
  };

  // Mark the symbol as moved/partially-moved if it is not
  // copyable.
  if (mark_moves and value_ast.To<IdentifierAst>() != nullptr and not copies) {
    mark_chain(var_sym, [&](const auto s) { s->MemInfo->MovedBy(value_ast, sm.CurrentScope); });
  }

  // Only whole moves carry up the chain. A pattern binding
  // an attribute out of the narrowed view partially moves
  // that view, but the value it narrows is still whole as
  // far as anything outside the branch is concerned - and
  // marking it otherwise makes the subject itself unreadable
  // at the "case" that introduced the narrowing.
  else if (mark_moves and value_ast.To<IdentifierAst>() == nullptr and not partial_copies) {
    var_sym->MemInfo->AstPartialMoves.EmplaceBack(&value_ast);
  }
}

auto spp::analyse::utils::mem_utils::RaiseIfMoved(
  VariableSymbol const &sym,
  Ast const &use,
  Scope *const scope)
  -> void {
  const auto [where_moved, _] = sym.MemInfo->AstMoved;
  if (where_moved == nullptr) { return; }
  const auto [where_init, _] = sym.MemInfo->AstInitializationOrigin;
  Raise<errors::SppUninitializedMemoryUseError>({scope}, ERR_ARGS(use, *where_init, *where_moved));
}

auto spp::analyse::utils::mem_utils::RaiseIfInconsistentlyMoved(
  VariableSymbol const &sym,
  Ast const &use,
  Scope *const scope)
  -> void {
  if (sym.MemInfo->IsInconsistentlyMoved.has_value()) {
    const auto [first, other] = *sym.MemInfo->IsInconsistentlyMoved;
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>({scope}, ERR_ARGS(use, *first, *other, "moved"));
  }
  if (sym.MemInfo->IsInconsistentlyPartiallyMoved.has_value()) {
    const auto [first, other] = *sym.MemInfo->IsInconsistentlyPartiallyMoved;
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
      {scope}, ERR_ARGS(use, *first, *other, "partially moved"));
  }
}
