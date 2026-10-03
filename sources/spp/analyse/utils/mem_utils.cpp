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
    /// [CHECKED]
    /// Raise if using "sym" here breaks an escaping borrow: one
    /// held of it (by a coroutine, a future or a closure), or
    /// one it holds itself.
    auto RaiseIfBadEscapingBorrow(
      VariableSymbol const &sym, Ast const &value_ast, Ast const &move_ast, const bool moves_value,
      SymbolMemoryOptions const &options, Scope const *scope) -> void {
      // Check we aren't trying to move a value something still
      // holds an escaping borrow of: the holder would be left
      // pointing at nothing. Todo: Allow inner scope moving?
      if (options.CheckMove and moves_value and not sym.MemInfo->AstContainersOfEscapingBorrows.IsEmpty()) {
        const auto [where_contained, _] = sym.MemInfo->AstContainersOfEscapingBorrows[0];
        Raise<errors::SppMovingEscapingBorrowedMemoryError>(
          {scope}, ERR_ARGS(*where_contained, move_ast));
      }

      // Copying a value out reads it, which a live "&mut"
      // escaping borrow of it rules out just as it rules out
      // borrowing it: the holder may be writing to it.
      if (options.CheckMove and not moves_value and not sym.MemInfo->AstContainersOfEscapingBorrows.IsEmpty()) {
        for (auto const *holder : scope->GetAllVarSymbols()) {
          for (auto const &[borrow, is_mut, _] : holder->MemInfo->AstContainedEscapingBorrows) {
            RaiseIf<errors::SppMemoryOverlapUsageError>(
              is_mut and regions::MemRegionOverlap(*borrow, value_ast),
              {scope}, ERR_ARGS(*borrow, value_ast));
          }
        }
      }

      // Check we aren't trying to move the container of an
      // escaping borrow (unless copyable). The container keeps
      // the borrows alive, so moving it propagates them out of
      // the frame that owns the borrowed values.
      if (options.CheckEscapingBorrowMove and options.CheckMove and moves_value
        and not sym.MemInfo->AstContainedEscapingBorrows.IsEmpty()) {
        Raise<errors::SppMovingEscapingBorrowedMemoryError>(
          {scope}, ERR_ARGS(*sym.Name, move_ast));
      }
    }

    /// [CHECKED]
    /// Specialized move check involving deferred values, as
    /// we have a special error. No value used in the analysis
    /// of the expression being deferred, can be uninitialised.
    auto RaiseIfMovedDeferred(
      VariableSymbol const &sym, meta::CompilerMetaData const *meta, Scope const *scope) -> void {
      // Get the where location, and if it is present, then
      // the value has been moved. In the defer context, throw.
      const auto where_moved = spp::get<0>(sym.MemInfo->AstMoved);
      if (where_moved and meta->DeferExit) {
        auto const &exit = *meta->DeferExit;
        Raise<errors::SppDeferConsumesMovedValueError>(
          {scope}, ERR_ARGS(*exit.Stmt, *where_moved, *exit.ExitPoint, sym.Name->Val, exit.ExitWhat));
      }
    }

    /// [CHECKED]
    /// Raise if "sym" was initialized, moved, partially moved,
    /// or holding an escaping borrow on only some of the paths
    /// reaching "usage" (the branches of a "case", the ways
    /// out of a loop).
    auto RaiseIfInconsistent(
      VariableSymbol const &sym, Ast const &usage, Scope const *scope) -> void {
      // If a value is inconsistently initialized, then throw an
      // error.
      if (sym.MemInfo->IsInconsistentlyInitialized.has_value()) {
        const auto [first, other] = *sym.MemInfo->IsInconsistentlyInitialized;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
          {scope}, ERR_ARGS(usage, *first, *other, "initialized"));
      }

      // If a value is inconsistently moved (initialized vs
      // moved), then throw an error.
      if (sym.MemInfo->IsInconsistentlyMoved.has_value()) {
        const auto [first, other] = *sym.MemInfo->IsInconsistentlyMoved;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
          {scope}, ERR_ARGS(usage, *first, *other, "moved"));
      }

      // If a value is inconsistently partially moved (partially
      // initialization status differs), then throw an error.
      if (sym.MemInfo->IsInconsistentlyPartiallyMoved.has_value()) {
        const auto [first, other] = *sym.MemInfo->IsInconsistentlyPartiallyMoved;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
          {scope}, ERR_ARGS(usage, *first, *other, "partially moved"));
      }

      // If a value inconsistently holds an escaping borrow, then
      // throw an error.
      if (sym.MemInfo->IsInconsistentlyBorrowEscaping.has_value()) {
        const auto [first, other] = *sym.MemInfo->IsInconsistentlyBorrowEscaping;
        Raise<errors::SppInconsistentlyEscapingBorrows>({scope}, ERR_ARGS(usage, *first, *other));
      }
    }
  }
}

auto spp::analyse::utils::mem_utils::ValidateSymbolMemory(
  ExpressionAst &value_ast, Ast const &move_ast,
  ScopeManager &sm,
  meta::CompilerMetaData *meta,
  SymbolMemoryOptions const &options) -> void {
  // For tuple and array literals, recursively analyse each
  // element, with all checks enabled, except the pass-through
  // "MarkMoves".
  if (const auto arr_literal = value_ast.To<ArrayLiteralRepeatedElementAst>(); arr_literal != nullptr) {
    const auto x = arr_literal->Elem.get();
    ValidateSymbolMemory(*x, move_ast, sm, meta, {.MarkMoves = options.MarkMoves});
    return;
  }
  if (const auto arr_literal = value_ast.To<ArrayLiteralExplicitElementsAst>(); arr_literal != nullptr) {
    for (auto &&x : arr_literal->Elems) {
      ValidateSymbolMemory(*x, move_ast, sm, meta, {.MarkMoves = options.MarkMoves});
    }
    return;
  }
  if (const auto tup_literal = value_ast.To<TupleLiteralAst>(); tup_literal != nullptr) {
    for (auto &&x : tup_literal->Elems) {
      ValidateSymbolMemory(*x, move_ast, sm, meta, {.MarkMoves = options.MarkMoves});
    }
    return;
  }

  // Get the symbol representing the outermost part of the
  // expression being moved. Non-symbolic => temporary value.
  auto [var_sym, var_scope] = sm.CurrentScope->FindVarSymbolOutermost(value_ast);
  if (var_sym == nullptr) { return; }
  const auto value_type_sym = value_ast.InferTypeRef(&sm, meta).Symbol;
  const auto outer_is_copyable = var_sym->TypeRefIn(*var_scope).Symbol->IsCopyable();
  const auto partial_is_copyable = value_type_sym != nullptr and value_type_sym->IsCopyable();

  // A move only occurs when the value is an identifier that's
  // not copyable, or a field that's not copyable (ie overall
  // not partially copyable).
  const auto moves_value = value_ast.To<IdentifierAst>() != nullptr
    ? not outer_is_copyable
    : not partial_is_copyable;

  // Check for inconsistent memory states (from branching),
  // deferred values being moved, general moves, escaping
  // borrows being moved.
  RaiseIfInconsistent(*var_sym, value_ast, sm.CurrentScope);
  RaiseIfMovedDeferred(*var_sym, meta, sm.CurrentScope);
  RaiseIfMoved(*var_sym, value_ast, sm.CurrentScope);
  RaiseIfBadEscapingBorrow(*var_sym, value_ast, move_ast, moves_value, options, sm.CurrentScope);

  // Check we aren't trying to move a comptime constant
  // (unless copyable).
  if (options.CheckMove and moves_value and var_sym->IsCompTime()) {
    Raise<errors::SppMovingCompTimeConstantMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, move_ast));
  }

  // Check the symbol doesn't have any outstanding partial
  // moved (moving a partially moved object).
  if (options.CheckPartialMove and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<IdentifierAst>() !=
    nullptr) {
    const auto [where_init, _] = var_sym->MemInfo->AstInitializationOrigin;
    const auto where_pm = var_sym->MemInfo->AstPartialMoves.Front();
    Raise<errors::SppPartiallyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
  }

  // Check the symbol doesn't have any outstanding partial
  // moves (directly moving a partial move).
  if (options.CheckPartialMove and not var_sym->MemInfo->AstPartialMoves.IsEmpty() and value_ast.To<IdentifierAst>() ==
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
          and (not options.IsPlaceWritten or path.Len() < steps.Len());
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
  if (options.CheckMoveFromBorrowedCtx and spp::get<0>(var_sym->MemInfo->AstBorrowed) and value_ast.To<
    IdentifierAst>() == nullptr and not partial_is_copyable) {
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
    for (auto *s = sym; s != nullptr; s = s->NarrowsSymbol.get()) { mark(s); }
  };

  // Mark the symbol as moved/partially-moved if it is not
  // copyable.
  if (options.MarkMoves and value_ast.To<IdentifierAst>() != nullptr and not outer_is_copyable) {
    mark_chain(var_sym, [&](const auto s) { s->MemInfo->MovedBy(value_ast, sm.CurrentScope); });
  }

  // Only whole moves carry up the chain. A pattern binding
  // an attribute out of the narrowed view partially moves
  // that view, but the value it narrows is still whole as
  // far as anything outside the branch is concerned - and
  // marking it otherwise makes the subject itself unreadable
  // at the "case" that introduced the narrowing.
  else if (options.MarkMoves and value_ast.To<IdentifierAst>() == nullptr and not partial_is_copyable) {
    var_sym->MemInfo->AstPartialMoves.EmplaceBack(&value_ast);
  }
}

auto spp::analyse::utils::mem_utils::RaiseIfMoved(
  VariableSymbol const &sym,
  Ast const &use,
  Scope const *scope)
  -> void {
  const auto [where_moved, _] = sym.MemInfo->AstMoved;
  if (where_moved == nullptr) { return; }
  const auto [where_init, _] = sym.MemInfo->AstInitializationOrigin;
  Raise<errors::SppUninitializedMemoryUseError>({scope}, ERR_ARGS(use, *where_init, *where_moved));
}
