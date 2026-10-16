module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.mem_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.borrows;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.identifier_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import genex;
import std;

namespace spp::analyse::utils::mem_utils {
  namespace {
    /// [CHECKED]
    /// Raise if using "sym" here breaks a loan: one of it (held by
    /// a coroutine, a future, a closure, or the binding a yielded
    /// or indexed borrow went to), or one it holds itself.
    auto RaiseIfBadEscapingBorrow(
      VariableSymbol const &sym, Ast const &value_ast, Ast const &move_ast, const bool moves_value,
      SymbolMemoryOptions const &options, ScopeManager const &sm) -> void {
      if (not options.CheckMove) { return; }
      auto const *const scope = sm.CurrentScope;
      auto &loans = memory_state::LoansOf(*scope);
      const auto is_on_sym = [&](memory_state::Loan const &loan) {
        return not loan.Pending and loan.Borrowed.Of.has_value() and loan.Borrowed.Of->Root == &sym;
      };

      // Check we aren't trying to move a value something still
      // holds a loan of: the holder would be left pointing at
      // nothing, if it is used again ("ConflictWith").
      if (moves_value) {
        for (auto &loan : loans) {
          if (not is_on_sym(loan)) { continue; }
          borrows::ConflictWith(
            loan, memory_state::LoanConflict{.Moved = true, .First = loan.Holder->Name.get(), .Second = &move_ast}, sm);
        }
      }

      // Copying a value out reads it, which a "&mut" loan of it
      // rules out just as it rules out borrowing it: the holder
      // may be writing to it. Only a name or an attribute path
      // reads the memory itself; a call, index or slice off it
      // is checked against the loans as the call it is.
      const auto reads_memory = value_ast.To<IdentifierAst>() != nullptr or regions::IsAttr(&value_ast, &sm);
      if (not moves_value and reads_memory) {
        const auto value_place = regions::PlaceAtOf(value_ast, *scope);
        for (auto &loan : loans) {
          if (not is_on_sym(loan) or not loan.IsMut or not regions::Overlap(loan.Borrowed, value_place)) { continue; }
          borrows::ConflictWith(
            loan, memory_state::LoanConflict{.Moved = false, .First = loan.Borrowed.Site, .Second = &value_ast}, sm);
        }
      }

      // Check we aren't trying to move the holder of a loan. It
      // keeps the borrows alive, so moving it propagates them out
      // of the frame that owns the borrowed values. A holder that
      // is itself a borrow (a pattern binding "&val", an index
      // borrow, a borrow a generator yielded) only passes the
      // borrow on, to whatever it is given to ("TransferLoans").
      const auto holds = memory_state::LoansHeldBy(loans, sym);
      const auto is_borrow = sym.MemInfo->AstBorrowed != nullptr
        or (sym.Type != nullptr and sym.Type->GetConvention() != nullptr);

      // A borrow binding passed on somewhere other than a call (which holds it only while it runs) - a block's or a
      // branch's result - leaves its loans with nothing tracking where the borrow went: they are live from here on.
      // Handed on by name ("let z = b"), the loans move to the new holder and are tracked again ("TransferLoans").
      // A narrowed view ("b" inside "case b of { is &mut S32(..) }") passes on what the symbol it narrows holds. The
      // binding itself is what is passed on (a borrow reads as "Copy" by what it points at, so "moves_value" does not
      // say this).
      const auto passes_on = options.MarkMoves and value_ast.To<IdentifierAst>() != nullptr;
      if (passes_on and is_borrow and move_ast.To<FunctionCallArgumentAst>() == nullptr) {
        for (auto const *s = &sym; s != nullptr; s = s->NarrowsSymbol.get()) {
          for (auto &loan : loans) {
            if (loan.Holder == s) { loan.Escaped = true; }
          }
        }
      }
      if (options.CheckEscapingBorrowMove and moves_value and not is_borrow
        and genex::any_of(holds, [](auto const *loan) { return not loan->Yielded; })) {
        Raise<errors::SppMovingEscapingBorrowedMemoryError>(
          {scope}, ERR_ARGS(*sym.Name, move_ast));
      }
    }

    /// [CHECKED]
    /// Specialized move check involving deferred values, as
    /// we have a special error. No value used in the analysis
    /// of the expression being deferred, can be uninitialised.
    auto RaiseIfMovedDeferred(
      VariableSymbol const &sym, Scope const *scope) -> void {
      // Get the where location, and if it is present, then
      // the value has been moved. In the defer context, throw.
      const auto where_moved = memory_state::StateOf(*scope, sym).AstMoved;
      auto const *const flow = memory_state::FlowOf(*scope);
      if (where_moved and flow != nullptr and flow->Deferring.has_value()) {
        auto const &exit = *flow->Deferring;
        Raise<errors::SppDeferConsumesMovedValueError>(
          {scope}, ERR_ARGS(*exit.Stmt, *where_moved, *exit.ExitPoint, sym.Name->Val, exit.ExitWhat));
      }
    }

    /// [CHECKED]
    /// Raise if "sym" was initialized, moved, partially moved,
    /// or holding an escaping borrow on only some of the paths
    /// reaching "usage" (the branches of a "case", the ways
    /// out of a loop). A part maybe moved only matters to a use
    /// that reaches it: "p.b" is whole on every path even where
    /// "p.a" was taken on one of them.
    auto RaiseIfInconsistent(
      VariableSymbol const &sym, Ast const &usage, SymbolMemoryOptions const &options, Scope const *scope) -> void {
      auto const &state = memory_state::StateOf(*scope, sym);
      if (const auto maybe = state.MaybeInit; maybe.has_value()) {
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
          {scope}, ERR_ARGS(usage, *maybe->First, *maybe->Other, "initialized"));
      }

      // A check of the value's presence alone (the object a member is read off) leaves its parts to the read itself.
      const auto used = regions::PlaceAtOf(usage, *scope);
      for (auto const &[part, paths] : options.CheckPartialMove
           ? state.MaybePartialMoves
           : Vec<memory_state::MaybeMovedPart>()) {
        const auto reaches = not used.Of.has_value()
          or regions::Relate(*used.Of, *part.Of) != regions::MemRegionRelation::Disjoint;
        RaiseIf<errors::SppInconsistentlyInitializedMemoryUseError>(
          reaches, {scope}, ERR_ARGS(usage, *paths.First, *paths.Other, "partially moved"));
      }

      // A loan of it, or one it holds, on only some of the paths that met.
      for (auto const &loan : memory_state::LoansOf(*scope)) {
        const auto involved = loan.Holder == &sym or (loan.Borrowed.Of.has_value() and loan.Borrowed.Of->Root == &sym);
        if (involved and loan.Maybe.has_value() and not loan.Pending) {
          Raise<errors::SppInconsistentlyEscapingBorrows>(
            {scope}, ERR_ARGS(usage, *loan.Maybe->First, *loan.Maybe->Other));
        }
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

  // A holder used again after a conflict with one of its
  // loans means the loan was live then (M9.2).
  borrows::RaiseIfConflictedUse(*var_sym, *sm.CurrentScope);
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
  RaiseIfInconsistent(*var_sym, value_ast, options, sm.CurrentScope);
  RaiseIfMovedDeferred(*var_sym, sm.CurrentScope);
  RaiseIfMoved(*var_sym, value_ast, sm.CurrentScope);
  RaiseIfBadEscapingBorrow(*var_sym, value_ast, move_ast, moves_value, options, sm);

  // Check we aren't trying to move a comptime constant
  // (unless copyable).
  if (options.CheckMove and moves_value and var_sym->IsCompTime()) {
    Raise<errors::SppMovingCompTimeConstantMemoryError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, move_ast));
  }

  // Check the symbol doesn't have any outstanding partial
  // moved (moving a partially moved object).
  auto const &state = memory_state::StateOf(*sm.CurrentScope, *var_sym);
  if (options.CheckPartialMove and not state.PartialMoves.empty() and value_ast.To<IdentifierAst>() != nullptr) {
    const auto where_init = state.AstInitializationOrigin;
    auto const *const where_pm = state.PartialMoves.front().Site;
    Raise<errors::SppPartiallyInitializedMemoryUseError>(
      {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
  }

  // Check the symbol doesn't have any outstanding partial
  // moves (directly moving a partial move).
  if (options.CheckPartialMove and not state.PartialMoves.empty() and value_ast.To<IdentifierAst>() == nullptr) {
    // "Contains" covers the move naming the place exactly
    // as well as the move naming something the place sits
    // inside of. Writing "o.inner.val" puts the first back
    // but cannot put "o.inner" back, so a write keeps only
    // the second.
    const auto place = regions::PlaceOf(value_ast, *sm.CurrentScope);
    const auto overlaps = state.PartialMoves
      | genex::views::filter([&](auto const &x) {
        return place.has_value()
          and regions::Relate(*x.Of, *place) == regions::MemRegionRelation::Contains
          and (not options.IsPlaceWritten or x.Of->Steps.size() < place->Steps.size());
      })
      | genex::to<Vec>();
    if (not overlaps.empty()) {
      const auto where_init = state.AstInitializationOrigin;
      auto const *const where_pm = overlaps.front().Site;
      Raise<errors::SppUninitializedMemoryUseError>(
        {sm.CurrentScope}, ERR_ARGS(value_ast, *where_init, *where_pm));
    }
  }

  // Check the symbol isn't being moved from a borrowed
  // context.
  if (options.CheckMoveFromBorrowedCtx and var_sym->MemInfo->AstBorrowed and value_ast.To<
    IdentifierAst>() == nullptr and not partial_is_copyable) {
    const auto where_borrow = var_sym->MemInfo->AstBorrowed;
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
    mark_chain(var_sym, [&](const auto s) { memory_state::StateOf(*sm.CurrentScope, *s).MovedBy(value_ast); });
  }

  // Only whole moves carry up the chain. A pattern binding
  // an attribute out of the narrowed view partially moves
  // that view, but the value it narrows is still whole as
  // far as anything outside the branch is concerned - and
  // marking it otherwise makes the subject itself unreadable
  // at the "case" that introduced the narrowing.
  else if (options.MarkMoves and value_ast.To<IdentifierAst>() == nullptr and not partial_is_copyable) {
    memory_state::StateOf(*sm.CurrentScope, *var_sym).PartMovedBy(regions::PlaceAtOf(value_ast, *sm.CurrentScope));
  }
}

/// [CHECKED]
auto spp::analyse::utils::mem_utils::RaiseIfMoved(
  VariableSymbol const &sym, Ast const &use, Scope const *scope) -> void {
  // Get the state of the symbol, and get where is was
  // moved (by ast). If it is not moved, return.
  auto const &state = memory_state::StateOf(*scope, sym);
  const auto where_moved = state.AstMoved;
  if (where_moved == nullptr) { return; }

  // Otherwise, get where it was initialised (prior to
  // being moved), and raise an error with the original
  // initialisation location, and the move location.
  const auto where_init = state.AstInitializationOrigin;
  Raise<errors::SppUninitializedMemoryUseError>(
    {scope}, ERR_ARGS(use, *where_init, *where_moved));
}

/// [CHECKED]
auto spp::analyse::utils::mem_utils::AssignmentTargetSymbol(
  Scope const &scope, meta::CompilerMetaData const &meta) -> VariableSymbol* {
  // Get the assignment target symbol off of the meta
  // context.
  if (meta.AssignmentTarget == nullptr) { return nullptr; }
  const auto declaring = meta.AssignmentTargetSymbol;

  // If not present, and an assignment target is, try
  // and query it off the scope.
  return declaring != nullptr and declaring->Name->NameId() == meta.AssignmentTarget->NameId()
    ? declaring
    : scope.FindVarSymbolOutermost(*meta.AssignmentTarget).first;
}
