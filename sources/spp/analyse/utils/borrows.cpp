module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.borrows;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.type_ast;
import genex;
import std;

namespace spp::analyse::utils::borrows {
  namespace {
    /// Check if one scope is the descendant of another (and not
    /// the same as the other), with a reverse ancestor lookup.
    auto StrictlyInside(Scope const *scope, Scope const *outer) -> bool {
      if (scope == nullptr or outer == nullptr or scope == outer) { return false; }
      return genex::position(scope->GetAncestors(), genex::operations::eq_fixed{outer}).has_value();
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::borrows::ConflictWith(
  memory_state::Loan &loan, memory_state::LoanConflict const &conflict,
  ScopeManager const &sm) -> void {
  // A loan "acts only when used" when it is borrowed or yielded,
  // or the symbol's type is a generator or closure. That is, it
  // doesn't do anything passively with the load, an action has
  // to occur - call the closure, resume the generator etc.
  const auto acts_only_when_used = [&] {
    const auto holder = loan.Holder;
    if (holder == nullptr or holder->Type == nullptr) { return false; }

    if (holder->MemInfo->AstBorrowed != nullptr
      or holder->Type->GetConvention() != nullptr
      or loan.Yielded) {
      return true;
    }

    // Check for a generator or closure type. Todo: what about
    // normal functions, could they trip this?
    const auto type = TypeRef::Of(*holder->Type, *sm.CurrentScope);
    return type.Symbol != nullptr and (
      type_predicates::IsTypeGenerator(type, *sm.CurrentScope)
      or type_predicates::IsTypeFunction(type, *sm.CurrentScope));
  };

  // If the loan is not escaped, and only acts when it's used,
  // set the conflict value on it to the conflict that was
  // passed into this function.
  if (not loan.Escaped and acts_only_when_used()) {
    if (not loan.Conflict.has_value()) { loan.Conflict = conflict; }
    return;
  }

  // TODO: Document.
  RaiseIf<errors::SppMovingEscapingBorrowedMemoryError>(
    conflict.Moved,
    {sm.CurrentScope}, ERR_ARGS(*conflict.First, *conflict.Second));
  Raise<errors::SppMemoryOverlapUsageError>(
    {sm.CurrentScope}, ERR_ARGS(*conflict.First, *conflict.Second));
}

/// [CHECKED]
auto spp::analyse::utils::borrows::RaiseIfConflictedUse(
  VariableSymbol const &holder, Scope const &scope) -> void {
  // Get the loans in the scope, and then filter them to the
  // loans held by the "holder".
  for (const auto loan : memory_state::LoansHeldBy(memory_state::LoansOf(scope), holder)) {
    if (not loan->Conflict.has_value()) { continue; }
    auto const &conflict = *loan->Conflict;

    // TODO: Document.
    RaiseIf<errors::SppMovingEscapingBorrowedMemoryError>(
      conflict.Moved,
      {&scope}, ERR_ARGS(*conflict.First, *conflict.Second));
    Raise<errors::SppMemoryOverlapUsageError>(
      {&scope}, ERR_ARGS(*conflict.First, *conflict.Second));
  }
}

/// [CHECKED]
auto spp::analyse::utils::borrows::CheckAgainstLoans(
  regions::PlaceAt const &place, const bool exclusive,
  memory_state::LoanSet &loans, memory_state::LoanSet const &call_loans,
  Ast const &use, ScopeManager const &sm) -> void {
  // The call's own borrows are all live while it runs;
  // enforce no overlap there.
  for (auto const &loan : call_loans) {
    spp::RaiseIf<errors::SppMemoryOverlapUsageError>(
      (exclusive or loan.IsMut) and regions::Overlap(loan.Borrowed, place), {sm.CurrentScope},
      ERR_ARGS(*loan.Borrowed.Site, use));
  }

  // A loan the body holds is live while its holder is
  // still to be used.
  for (auto &loan : loans) {
    if (loan.Pending or not((exclusive or loan.IsMut) and regions::Overlap(loan.Borrowed, place))) { continue; }
    ConflictWith(loan, memory_state::LoanConflict{.Moved = false, .First = loan.Borrowed.Site, .Second = &use}, sm);
  }
}

/// [CHECKED]
auto spp::analyse::utils::borrows::ValidateUnnamedArgumentBorrow(
  FunctionCallArgumentAst const &arg, VariableSymbol const *const sym, memory_state::LoanSet &call_loans,
  ScopeManager &sm, meta::CompilerMetaData *const meta) -> void {
  // A borrow of a named symbol is checked with the symbol; this
  // handles unnamed borrow creators, like vec[mut 5] etc.
  if (sym != nullptr) { return; }

  // Get the convention from the inference. For owned symbols,
  // return too.
  const auto conv = arg.Conv != nullptr
    ? arg.Conv->Tag()
    : arg.Val->InferTypeRef(&sm, meta).Conv;
  if (conv == ConventionTag::MOV) { return; }

  // A borrow of a linear temporary leaves nothing holding the
  // value once the call is done.
  if (linear_utils::IsLinearTemporary(*arg.Val, sm, meta)) {
    linear_utils::RaiseTemporaryNotConsumed(
      *arg.Val, arg, "Borrowed here, so nothing owns it after this expression", sm, meta);
  }

  // Cross check with the loans held and the call's own borrows,
  // raising an error on conflict, and hold this one too, so the
  // following borrows are checked against it.
  const auto is_mut = conv == ConventionTag::MUT;
  const auto borrowed = regions::PlaceAtOf(*arg.Val, *sm.CurrentScope);
  CheckAgainstLoans(
    borrowed, is_mut, memory_state::LoansOf(*sm.CurrentScope), call_loans, *arg.Val, sm);
  call_loans.emplace_back(memory_state::MakeLoan(borrowed, is_mut));
}

auto spp::analyse::utils::borrows::EnforceBorrowsOutliveHolder(
  VariableSymbol const &holder, VariableSymbol const *from,
  Ast const &owner, ScopeManager const &sm) -> void {

  // Create a combined list of loans held by the provided
  // "holder" symbol and the "from" symbol (combined).
  auto const &loans = memory_state::LoansOf(*sm.CurrentScope);
  auto carried = memory_state::LoansHeldBy(loans, holder);
  if (from != nullptr) { carried.append_range(memory_state::LoansHeldBy(loans, *from)); }

  // For each loan in the carried list, check that the
  // borrow definition scope is strictly inside the scope
  // of the holder symbol.
  for (const auto loan : carried) {
    const auto root = loan->Borrowed.Of.has_value() ? loan->Borrowed.Of->Root : nullptr;
    RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
      root != nullptr and StrictlyInside(root->ScopeDefinedIn, holder.ScopeDefinedIn), {sm.CurrentScope},
      ERR_ARGS(owner, *holder.Name, *loan->Borrowed.Site));
  }

  // A borrow binding given on by name that holds no loan
  // (a pattern's "&val", a parameter) is valid no longer
  // than itself, so the holder may not outlive it either.
  if (from == nullptr or from->MemInfo->AstBorrowed == nullptr) { return; }
  if (not memory_state::LoansHeldBy(loans, *from).empty()) { return; }
  RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
    StrictlyInside(from->ScopeDefinedIn, holder.ScopeDefinedIn), {sm.CurrentScope},
    ERR_ARGS(owner, *holder.Name, *from->MemInfo->AstBorrowed));
}
