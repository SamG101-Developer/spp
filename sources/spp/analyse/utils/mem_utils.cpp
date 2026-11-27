module;
#include <spp/macros.hpp>
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
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.identifier_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.spread_expression_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts, ArrayLiteralExplicitElementsAst);
use_alias(spp::asts, ArrayLiteralRepeatedElementAst);
use_alias(spp::asts, Ast);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, FunctionCallArgumentAst);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, SpreadExpressionAst);
use_alias(spp::asts, TupleLiteralAst);
use_alias(spp::asts::meta, CompilerMetaData);

namespace spp::analyse::utils::mem_utils {
  /// Raise if using "sym" here breaks a loan: one of it (held by
  /// a coroutine, a future, a closure, or the binding a yielded
  /// or indexed borrow went to), or one it holds itself. "named" is
  /// what the use names: "sym" itself, or a view of it, whose being
  /// a borrow is what says one is passed on.
  static auto RaiseIfBadEscapingBorrow(
    VariableSymbol const &sym, VariableSymbol const &named, Ast const &value_ast, Ast const &move_ast,
    bool moves_value, SymbolMemoryOptions const &options, ScopeManager const &sm) -> void;


  /// Raise if "sym" was initialized, moved, partially moved,
  /// or holding an escaping borrow on only some of the paths
  /// reaching "usage" (the branches of a "case", the ways
  /// out of a loop). A part maybe moved only matters to a use
  /// that reaches it: "p.b" is whole on every path even where
  /// "p.a" was taken on one of them.
  static auto RaiseIfInconsistent(
    VariableSymbol const &sym, Ast const &usage, SymbolMemoryOptions const &options, Scope const *scope) -> void;
}

/// [CHECKED]
auto spp::analyse::utils::mem_utils::RaiseIfBadEscapingBorrow(
  VariableSymbol const &sym, VariableSymbol const &named, Ast const &value_ast, Ast const &move_ast,
  const bool moves_value, SymbolMemoryOptions const &options, ScopeManager const &sm) -> void {
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
  const auto is_borrow = named.IsBorrow();

  // A borrow binding passed on somewhere other than a call (which holds it only while it runs) - a block's or a
  // branch's result - leaves its loans with nothing tracking where the borrow went: they are live from here on.
  // Handed on by name ("let z = b"), the loans move to the new holder and are tracked again ("TransferLoans").
  // A view ("b" narrowed inside "case b of { is &mut S32(..) }") is its owner here, so its owner's loans are the ones
  // passed on. The binding itself is what is passed on (a borrow reads as "Copy" by what it points at, so
  // "moves_value" does not say this).
  const auto passes_on = options.MarkMoves and value_ast.To<IdentifierAst>() != nullptr;
  if (passes_on and is_borrow and move_ast.To<FunctionCallArgumentAst>() == nullptr) {
    for (auto &loan : loans) {
      if (loan.Holder == &sym) { loan.Escaped = true; }
    }
  }
  if (options.CheckEscapingBorrowMove and moves_value and not is_borrow
    and genex::any_of(holds, [](auto const *loan) { return not loan->Yielded; })) {
    Raise<errors::SppMovingEscapingBorrowedMemoryError>(ERR_ARGS(*sym.Name, move_ast));
  }
}

/// [CHECKED]
auto spp::analyse::utils::mem_utils::RaiseIfInconsistent(
  VariableSymbol const &sym, Ast const &usage, SymbolMemoryOptions const &options, Scope const *scope) -> void {
  auto const &state = memory_state::StateOf(*scope, sym);
  if (const auto maybe = state.MaybeInit; maybe.has_value()) {
    Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
      ERR_ARGS(usage, *maybe->First, *maybe->Other, "initialized"));
  }

  // A check of the value's presence alone (the object a member is read off) leaves its parts to the read itself.
  const auto used = regions::PlaceAtOf(usage, *scope);
  for (auto const &[part, paths] : options.CheckPartialMove
       ? state.MaybePartialMoves
       : Vec<memory_state::MaybeMovedPart>()) {
    const auto reaches = not used.Of.has_value()
      or regions::Relate(*used.Of, *part.Of) != regions::MemRegionRelation::Disjoint;
    RaiseIf<errors::SppInconsistentlyInitializedMemoryUseError>(
      reaches, ERR_ARGS(usage, *paths.First, *paths.Other, "partially moved"));
  }

  // A loan of it, or one it holds, on only some of the paths that met.
  for (auto const &loan : memory_state::LoansOf(*scope)) {
    const auto involved = loan.Holder == &sym or (loan.Borrowed.Of.has_value() and loan.Borrowed.Of->Root == &sym);
    if (involved and loan.Maybe.has_value() and not loan.Pending) {
      Raise<errors::SppInconsistentlyEscapingBorrows>(ERR_ARGS(usage, *loan.Maybe->First, *loan.Maybe->Other));
    }
  }
}

auto spp::analyse::utils::mem_utils::ValidateSymbolMemory(
  ExpressionAst &value_ast, Ast const &move_ast, ScopeManager &sm,
  CompilerMetaData *meta, SymbolMemoryOptions const &options) -> void {
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

  // "(x)" is "x" ("asts::Unparenthesised"): moving it moves "x".
  if (const auto paren = value_ast.To<asts::ParenthesisedExpressionAst>(); paren != nullptr) {
    ValidateSymbolMemory(*paren->Expr, move_ast, sm, meta, options);
    return;
  }

  // A spread in a tuple literal ("(..t)") moves the tuple it spreads.
  if (const auto spread = value_ast.To<SpreadExpressionAst>(); spread != nullptr) {
    ValidateSymbolMemory(*spread->Expr, move_ast, sm, meta, options);
    return;
  }

  // Get the symbol representing the outermost part of the
  // expression being moved. Non-symbolic => temporary value.
  auto [var_sym, var_scope] = sm.CurrentScope->FindVarSymbolOutermost(value_ast);
  if (var_sym == nullptr) { return; }

  // A view ("regions::IsView", an "is" binding) is a name for a part
  // of its owner: what is read or moved is that part of the owner, so
  // the owner is what is checked and marked, at the place the view
  // resolves to. A variant's payload is addressed through the member
  // it holds ("o.<Some[T]>.val"); a path into a variant that names no
  // member cannot be followed, so using it uses the variant whole.
  // A view reached through a borrow (a borrowed member: "case a of {
  // is &A(..) }") is behind that borrow, though its owner is not.
  auto view_place = Opt<regions::Place>();
  auto const &named = *var_sym;
  auto const *where_borrowed = var_sym->MemInfo->AstBorrowed;
  for (auto const *link = var_sym; where_borrowed == nullptr and regions::IsView(*link); link = link->ViewRoot) {
    where_borrowed = link->ViewRoot->MemInfo->AstBorrowed;
  }
  if (regions::IsView(*var_sym)) {
    view_place = regions::PlaceOf(value_ast, *sm.CurrentScope);
    if (not view_place.has_value()) {
      auto *root = var_sym;
      while (regions::IsView(*root)) { root = root->ViewRoot; }
      view_place = regions::Place{.Root = root};
    }
    var_sym = view_place->Root;
    var_scope = var_sym->ScopeDefinedIn;
    const auto root_ref = var_sym->TypeRefIn(*var_scope);
    if (not view_place->Steps.empty() and type_predicates::IsTypeVariant(root_ref, *var_scope)
      and not type_members::VariantMemberAt(root_ref, view_place->Steps.front().Name, *var_scope).has_value()) {
      view_place->Steps.clear();
    }
  }
  const auto names_whole = view_place.has_value()
    ? view_place->Steps.empty()
    : value_ast.To<IdentifierAst>() != nullptr;

  // A holder used again after a conflict with one of its
  // loans means the loan was live then (M9.2).
  borrows::RaiseIfConflictedUse(*var_sym, *sm.CurrentScope);

  // A part that is a borrow (an element of a borrowed pack, "xs.0" for
  // "..xs: &Ts"; a field never is one) passes the borrow on, so moves nothing.
  const auto value_ref = value_ast.InferTypeRef(&sm, meta);
  const auto value_type_sym = value_ref.Symbol;
  const auto outer_is_copyable = var_sym->TypeRefIn(*var_scope).Symbol->IsCopyable();
  const auto partial_is_copyable = value_ref.IsBorrowed() or (
    value_type_sym != nullptr and value_type_sym->IsCopyable());

  // A move only occurs when the value is an identifier that's
  // not copyable, or a field that's not copyable (ie overall
  // not partially copyable).
  // A view's own type says whether reading it copies: a "Copy"
  // payload of a variant that is not "Copy" is copied out of it.
  const auto moves_value = names_whole and not view_place.has_value()
    ? not outer_is_copyable
    : not partial_is_copyable;

  // Check for inconsistent memory states (from branching),
  // deferred values being moved, general moves, escaping
  // borrows being moved.
  RaiseIfInconsistent(*var_sym, value_ast, options, sm.CurrentScope);
  RaiseIfMoved(*var_sym, value_ast, sm.CurrentScope);
  RaiseIfBadEscapingBorrow(*var_sym, named, value_ast, move_ast, moves_value, options, sm);

  // Check we aren't trying to move a comptime constant
  // (unless copyable).
  if (options.CheckMove and moves_value and var_sym->IsCompTime()) {
    Raise<errors::SppMovingCompTimeConstantMemoryError>(ERR_ARGS(value_ast, move_ast));
  }

  // Check the symbol doesn't have any outstanding partial
  // moved (moving a partially moved object).
  auto const &state = memory_state::StateOf(*sm.CurrentScope, *var_sym);
  if (options.CheckPartialMove and not state.PartialMoves.empty() and names_whole) {
    const auto where_init = state.AstInitializationOrigin;
    auto const *const where_pm = state.PartialMoves.front().Site;
    Raise<errors::SppPartiallyInitializedMemoryUseError>(ERR_ARGS(value_ast, *where_init, *where_pm));
  }

  // Check the symbol doesn't have any outstanding partial
  // moves (directly moving a partial move).
  if (options.CheckPartialMove and not state.PartialMoves.empty() and not names_whole) {
    // "Contains" covers the move naming the place exactly
    // as well as the move naming something the place sits
    // inside of. Writing "o.inner.val" puts the first back
    // but cannot put "o.inner" back, so a write keeps only
    // the second.
    const auto place = view_place.has_value() ? view_place : regions::PlaceOf(value_ast, *sm.CurrentScope);
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
      Raise<errors::SppUninitializedMemoryUseError>(ERR_ARGS(value_ast, *where_init, *where_pm));
    }
  }

  // Check the symbol isn't being moved from a borrowed
  // context.
  if (options.CheckMoveFromBorrowedCtx and where_borrowed != nullptr
    and (view_place.has_value() or value_ast.To<IdentifierAst>() == nullptr) and not partial_is_copyable) {
    Raise<errors::SppMoveFromBorrowedMemoryError>(ERR_ARGS(value_ast, *where_borrowed, *where_borrowed));
  }

  // Mark what was taken: the whole symbol, or the part of it named
  // (a view's owner, at the place the view resolves to, above).
  if (options.MarkMoves and moves_value and names_whole) {
    memory_state::StateOf(*sm.CurrentScope, *var_sym).MovedBy(value_ast);
  }
  else if (options.MarkMoves and moves_value) {
    const auto place = view_place.has_value()
      ? regions::PlaceAt(&value_ast, view_place)
      : regions::PlaceAtOf(value_ast, *sm.CurrentScope);
    memory_state::StateOf(*sm.CurrentScope, *var_sym).PartMovedBy(place);
  }
}

auto spp::analyse::utils::mem_utils::TakeApart(
  VariableSymbol const &sym, Ast const &place, Ast const &by,
  const bool disarms, ScopeManager const &sm) -> void {
  // A view ("regions::IsView") is taken apart in its owner: the place
  // resolves to the owner's ("o.<Some[T]>.val"), which is what is marked.
  const auto region = regions::PlaceOf(place, *sm.CurrentScope);
  auto const &owner = region.has_value() ? *region->Root : sym;
  auto &state = memory_state::StateOf(*sm.CurrentScope, owner);
  if (region.has_value() and not regions::FirstUnaccountedPart(owner, *region, sm).empty()) {
    if (disarms) { state.TakenApartAt(regions::PlaceAt(&place, region)); }
    return;
  }
  const auto whole = region.has_value() ? region->Steps.empty() : place.To<IdentifierAst>() != nullptr;
  if (whole) { state.MovedBy(by); }
  else { state.PartMovedBy(regions::PlaceAt(&place, region)); }
}

/// [CHECKED]
auto spp::analyse::utils::mem_utils::RaiseIfMoved(
  VariableSymbol const &sym, Ast const &use, Scope const *scope) -> void {
  // Held on only some of the paths that met is not held either.
  auto const &state = memory_state::StateOf(*scope, sym);
  const auto maybe = state.MaybeInit;
  RaiseIf<errors::SppInconsistentlyInitializedMemoryUseError>(
    maybe.has_value(),
    ERR_ARGS(use, *maybe->First, *maybe->Other, "initialized"));

  // Get where it was moved (by ast). If it is not moved, return.
  const auto where_moved = state.AstMoved;
  if (where_moved == nullptr) { return; }

  // Otherwise, get where it was initialised (prior to
  // being moved), and raise an error with the original
  // initialisation location, and the move location.
  const auto where_init = state.AstInitializationOrigin;
  Raise<errors::SppUninitializedMemoryUseError>(
    ERR_ARGS(use, *where_init, *where_moved));
}

/// [CHECKED]
auto spp::analyse::utils::mem_utils::AssignmentTargetSymbol(
  Scope const &scope, CompilerMetaData const &meta) -> VariableSymbol* {
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
