module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.memory_state;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.regions;
import spp.asts.ast;
import spp.utils.ice;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::memory_state {
  namespace {
    /// What a borrow is compared by across paths: its place (the
    /// root variable and steps, so a shadowed name is another
    /// place), or what it was written as if it names none. Each
    /// branch of a "case" builds its own asts, so the same borrow
    /// written in two branches is two pointers but one place.
    using BorrowKey = Variant<regions::Place, Str>;
    auto BorrowKeyOf(
      regions::PlaceAt const &borrow) -> BorrowKey {
      return borrow.Of.has_value()
        ? BorrowKey(*borrow.Of)
        : BorrowKey(borrow.Site->ToString());
    }

    /// A loan, compared across paths: its holder, the place
    /// borrowed, and how.
    auto KeyOf(Loan const &loan) -> Tup<VariableSymbol const*, BorrowKey, bool, bool> {
      return {loan.Holder, BorrowKeyOf(loan.Borrowed), loan.IsMut, loan.Yielded};
    }

    /// The loan in "loans" that is "loan", if any.
    auto FindLoan(LoanSet &loans, Loan const &loan) -> Loan* {
      const auto key = KeyOf(loan);
      for (auto &x : loans) {
        if (KeyOf(x) == key) { return &x; }
      }
      return nullptr;
    }

    /// Whether two partial moves took the same part: the same
    /// place ("PartMovedBy" records none without one).
    auto SamePart(regions::PlaceAt const &lhs, regions::PlaceAt const &rhs) -> bool {
      return *lhs.Of == *rhs.Of;
    }

    /// [CHECKED]
    /// Whether "parts" holds "part". A "contains" function with
    /// a custom equality test ("SamePart").
    auto HoldsPart(
      Vec<regions::PlaceAt> const &parts, regions::PlaceAt const &part) -> bool {
      return genex::any_of(parts, [&](auto const &x) { return SamePart(x, part); });
    }

    /// [CHECKED]
    /// Record "part" as maybe moved, between "paths", unless
    /// it already is. An "add element if unique", with a custom
    /// equality test ("SamePart").
    auto AddMaybePart(
      Vec<MaybeMovedPart> &maybe, regions::PlaceAt const &part, PathsDisagree const &paths) -> void {
      if (genex::any_of(maybe, [&](auto const &x) { return SamePart(x.Part, part); })) { return; }
      maybe.emplace_back(MaybeMovedPart{.Part = part, .Paths = paths});
    }
  }
}

SPP_MOD_BEGIN
auto MemoryState::InitializedBy(
  Ast const &ast)
  -> void {
  // Set the initialisation asts, and clean the "moved" ast.
  // This state has been "given" a value.
  AstInitialization = &ast;
  AstInitializationOrigin = &ast;
  AstMoved = nullptr;
  Given = true;

  // Clear the "maybe init" flag, nothing is owed as this is
  // a fresh value, and there are no partial moves.
  // Todo: PartialMoves.clear()?
  MaybeInit = Nullopt;
  OwesNothing = false;
  MaybePartialMoves.clear();
}

auto MemoryState::MovedBy(
  Ast const &ast) -> void {
  // Bind the move to the field, and clear the initialisation
  // ast too.
  AstMoved = &ast;
  AstInitialization = nullptr;

  // Clear the partial moves, the "maybe init" and the "maybe
  // partial moves" too.
  PartialMoves.clear();
  MaybeInit = Nullopt;
  MaybePartialMoves.clear();
}

auto MemoryState::PartMovedBy(
  regions::PlaceAt const &part) -> void {
  // A partial move is of an attribute chain rooted at a symbol,
  // which always has a place: one without is a bug upstream.
  if (not part.Of.has_value()) { Ice(*part.Site, "a partial move with no place"); }

  // Taken on every path from here, so whatever the paths
  // before took inside it no longer matters.
  MaybePartialMoves |= genex::actions::remove_if([&](auto const &maybe) {
    return regions::Relate(*part.Of, *maybe.Part.Of) == regions::MemRegionRelation::Contains;
  });
  PartialMoves.emplace_back(part);
}

/// [CHECKED]
auto MemoryState::RemovePartialMoves(
  Ast const &ast, Scope const *scope) -> void {
  // Determine a "place" for the ast to represent the root
  // symbol and sny steps into it. If there is no scope, the
  // region cannot be completed => nullopt.
  const auto written = scope != nullptr
    ? regions::PlaceOf(ast, *scope)
    : Nullopt;

  // A partial move is written over if what is written is its
  // place. Writing something that is no place (a temporary's
  // attribute) puts nothing back.
  const auto is_written = [&](regions::PlaceAt const &move) {
    return written.has_value() and *move.Of == *written;
  };

  // Filter off the partial moves that are written. Do the
  // same for the "maybe partially moved" values too.
  PartialMoves |= genex::actions::remove_if(is_written);
  MaybePartialMoves |= genex::actions::remove_if(
    [&](auto const &maybe) { return is_written(maybe.Part); });
}

BodyFlow::BodyFlow(
  Scope const &body) :
  _Body(&body),
  _Outer(std::exchange(body.Flow, MakeShared<FlowState>())) {
}

BodyFlow::~BodyFlow() {
  _Body->Flow = std::move(_Outer);
}

auto MemoryInfo::Clone() const
  -> Unique<MemoryInfo> {
  auto out = MakeUnique<MemoryInfo>();
  static_cast<MemoryState&>(*out) = *this;
  out->AstBorrowed = AstBorrowed;
  return out;
}

SPP_MOD_END

auto spp::analyse::utils::memory_state::SnapshotSymbols(
  Vec<VariableSymbol*> const &syms, Scope const &scope) -> ScopeSnapshot {
  // Create an empty snapshot and load it with the symbols and
  // the symbols' states.
  auto out = ScopeSnapshot();
  for (const auto sym : syms) {
    out.Symbols.emplace_back(sym->SharedFromThis<VariableSymbol>(), StateOf(scope, *sym));
  }

  // Assign the "flow" of the scope into snapshot. This can be
  // nullptr if scope isn't in a function. Otherwise, bring
  // across the symbols' states and the loans.
  out.In = FlowOf(scope);
  if (out.In != nullptr) {
    out.States = out.In->States;
    out.Loans = out.In->Loans;
  }
  return out;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::SnapshotFlow(
  Scope const &scope) -> ScopeSnapshot {
  // Create a new snapshot object, and assign the scope's
  // "flow" into the snapshot (pointer).
  auto out = ScopeSnapshot();
  out.In = FlowOf(scope);

  // If the scope had no "flow", then snapshot the symbols
  // from the scope into the scope's "flow".
  if (out.In == nullptr) {
    return SnapshotSymbols(scope.GetAllVarSymbols(), scope);
  }

  // Copy the symbols' states and the loans from the "flow"
  // to the scope.
  out.States = out.In->States;
  out.Loans = out.In->Loans;
  return out;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::RestoreSnapshot(
  ScopeSnapshot const &snapshot) -> void {
  // Inside a body every symbol state and loan goes back
  // (not the walk's loop and "defer" bookkeeping); outside
  // one, each symbol's own state.
  if (snapshot.In != nullptr) {
    snapshot.In->States = snapshot.States;
    snapshot.In->Loans = snapshot.Loans;
    return;
  }

  // Move the state off the snapshot map into the symbol
  // itself.
  for (auto const &[sym, state] : snapshot.Symbols) {
    static_cast<MemoryState&>(*sym->MemInfo) = state;
  }
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::Join(
  MemoryState &into, MemoryState const &other, Ast *into_path, Ast *other_path) -> void {
  // A path that already disagreed within itself (a "case"
  // nested in it) still disagrees once it meets the others.
  const auto here = PathsDisagree{.First = into_path, .Other = other_path};
  if (not into.MaybeInit) { into.MaybeInit = other.MaybeInit; }
  for (auto const &maybe : other.MaybePartialMoves) {
    AddMaybePart(into.MaybePartialMoves, maybe.Part, maybe.Paths);
  }

  // Nothing is owed where every path either moved the value
  // or holds only a "Copy" one. Read before the witnesses.
  const auto into_moved = into.AstInitialization == nullptr;
  const auto other_moved = other.AstInitialization == nullptr;
  const auto owes_nothing = (into_moved or into.OwesNothing)
    and (other_moved or other.OwesNothing);

  // The "into" state is "maybe initialised" if there is a
  // mismatch in the initialisation or move state in the
  // two states.
  if ((into.AstInitialization == nullptr) != (other.AstInitialization == nullptr)
    or (into.AstMoved == nullptr) != (other.AstMoved == nullptr)) {
    into.MaybeInit = here;
  }

  // The partial moves are lists so need a 2-way check on
  // existing on one side and not the other. Add the unique
  // parts from the "other" state, then remove states found
  // in both, leaving uniquely "into" or "other" states.
  for (auto const &part : other.PartialMoves) {
    if (HoldsPart(into.PartialMoves, part)) { continue; }
    AddMaybePart(into.MaybePartialMoves, part, here);
  }
  into.PartialMoves |= genex::actions::remove_if([&](auto const &part) {
    if (HoldsPart(other.PartialMoves, part)) { return false; }
    AddMaybePart(into.MaybePartialMoves, part, here);
    return true;
  });

  // An immutable given its value on any path cannot be given
  // another. The "given" is the initial initialisation so
  // from one path at least.
  into.Given = into.Given or other.Given;
  into.OwesNothing = owes_nothing;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::SameShape(
  MemoryState const &lhs, MemoryState const &rhs) -> bool {
  const auto parts = [](auto const &list, auto const &part_of) {
    // The parts of the elements in the list, by place,
    // transformed against the "part_of" extractor.
    auto out = list
      | genex::views::transform([&](auto const &x) { return BorrowKeyOf(part_of(x)); })
      | genex::to<Vec>();
    genex::actions::sort(out);
    return out;
  };

  // Define the transformation functions for different
  // place types.
  const auto self = [](regions::PlaceAt const &x) -> regions::PlaceAt const& { return x; };
  const auto maybe_part = [](MaybeMovedPart const &x) -> regions::PlaceAt const& { return x.Part; };

  // Compare all the different parts of the state objects.
  return (lhs.AstInitialization == nullptr) == (rhs.AstInitialization == nullptr)
    and (lhs.AstMoved == nullptr) == (rhs.AstMoved == nullptr)
    and lhs.MaybeInit.has_value() == rhs.MaybeInit.has_value()
    and lhs.OwesNothing == rhs.OwesNothing
    and parts(lhs.PartialMoves, self) == parts(rhs.PartialMoves, self)
    and parts(lhs.MaybePartialMoves, maybe_part) == parts(rhs.MaybePartialMoves, maybe_part);
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::MakeLoan(
  regions::PlaceAt borrowed, const bool is_mut, VariableSymbol *holder,
  const bool yielded, const bool pending) -> Loan {
  // Build a loan based on the parameters passed into this
  // function. This represents an escaping borrow.
  auto loan = Loan();
  loan.Borrowed = std::move(borrowed);
  loan.IsMut = is_mut;
  loan.Holder = holder;
  loan.Yielded = yielded;
  loan.Pending = pending;
  return loan;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::FlowOf(
  Scope const &scope) -> FlowState* {
  // Move up through a scopes ancestors, until a flow is
  // found, starting in the given scope. Outside any body there
  // is none.
  for (auto at = &scope; at != nullptr; at = at->Parent) {
    if (at->Flow != nullptr) { return at->Flow.get(); }
  }
  return nullptr;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::StateOf(
  Scope const &scope, VariableSymbol const &sym) -> MemoryState& {
  // Get the "flow" from the scope, and either get the symbols
  // cached state, or look it up in the flow. The flow is
  // nullptr when we aren't in a function/body.
  const auto flow = FlowOf(scope);
  if (flow == nullptr) { return *sym.MemInfo; }
  if (const auto found = flow->States.find(&sym); found != flow->States.end()) {
    return found->second;
  }

  // Otherwise, emplace a copy of the symbols state into the
  // "flow", and return the copied state as a reference.
  return flow->States.emplace(
    &sym, static_cast<MemoryState const&>(*sym.MemInfo)).first->second;
}

/// [CHECKED] - Fallbacks & simplify?
auto spp::analyse::utils::memory_state::StateIn(
  ScopeSnapshot const &snapshot, VariableSymbol const &sym) -> MemoryState {
  // Look for the symbol in the snapshot. If it is found in
  // there, get the symbol;s memory and slice it.
  if (snapshot.In != nullptr) {
    const auto found = snapshot.States.find(&sym);
    return found != snapshot.States.end()
      ? found->second
      : static_cast<MemoryState const&>(*sym.MemInfo);
  }

  // Otherwise, loop through the symbols in the snapshot,
  // look for a symbol match by pointer and return the
  // associated state.
  for (auto const &[s, state] : snapshot.Symbols) {
    if (s.get() == &sym) { return state; }
  }

  // Final fallback: take the info from the symbol directly.
  return static_cast<MemoryState const&>(*sym.MemInfo);
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::LoansOf(
  Scope const &scope) -> LoanSet& {
  // Outside any body (a module-level constant), nothing outlives
  // the expression, but the checks still need a set.
  static auto outside_any_body = LoanSet();
  const auto flow = FlowOf(scope);
  return flow != nullptr ? flow->Loans : outside_any_body;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::ActivateLoans(
  LoanSet &loans, VariableSymbol const &holder) -> void {
  // Move all the defined escaping borrows out of their pending
  // state.
  for (auto &loan : loans) {
    if (loan.Holder == &holder) { loan.Pending = false; }
  }
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::LoansHeldBy(
  LoanSet const &loans, VariableSymbol const &holder) -> Vec<Loan const*> {
  // Get all the escaping borrows whose holder matches the
  // "holder", and are not pending.
  auto out = Vec<Loan const*>();
  for (auto const &loan : loans) {
    if (loan.Holder == &holder and not loan.Pending) { out.emplace_back(&loan); }
  }
  return out;
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::ReleaseLoans(
  LoanSet &loans, VariableSymbol const &holder) -> void {
  // Remove and escaping borrows being held by the inputted
  // "holder".
  loans |= genex::actions::remove_if([&](auto const &loan) { return loan.Holder == &holder; });
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::TransferLoans(
  LoanSet &loans, VariableSymbol const &from, VariableSymbol &to,
  const bool pending) -> void {
  // For each load, if the holder is the "from" symbol, remap
  // the holder to the "to" symbol, and set the "pending" and
  // "escaped" flag.
  for (auto &loan : loans) {
    if (loan.Holder != &from) { continue; }
    loan.Holder = &to;
    loan.Pending = pending;
    loan.Escaped = false;
  }
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::JoinLoans(
  LoanSet &into, LoanSet const &other, Ast *into_path,
  Ast *other_path) -> void {
  // A loan held on either path is held after them; one held on
  // only one of them is held maybe.
  const auto here = PathsDisagree{.First = into_path, .Other = other_path};
  auto other_copy = other;

  // Iterate each loan for "into", and check for state agreement
  // (joined result), or a disagreement.
  for (auto &loan : into) {
    // Look for the load in the "other" list. If it is not
    // present, then it is a disagreement: "maybe".
    const auto match = FindLoan(other_copy, loan);
    if (match == nullptr) {
      if (not loan.Maybe.has_value()) { loan.Maybe = here; }
    }

    // Otherwise, copy over critical information for the memory
    // integrity,
    else {
      if (not loan.Maybe.has_value()) { loan.Maybe = match->Maybe; }
      if (not loan.Conflict.has_value()) { loan.Conflict = match->Conflict; }
      loan.Escaped = loan.Escaped or match->Escaped;
    }
  }

  // Then, get the loans from the other list, and if they are
  // not in the "into" list, then they are unique, so set them
  // to "maybe" loaned, and add them to "into".
  for (auto const &loan : other) {
    if (FindLoan(into, loan) != nullptr) { continue; }
    auto added = loan;
    if (not added.Maybe.has_value()) { added.Maybe = here; }
    into.emplace_back(std::move(added));
  }
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::SameLoans(
  LoanSet const &lhs, LoanSet const &rhs) -> bool {
  // A function to transform each loan into a key of unique
  // and critical information.
  const auto keys = [](LoanSet const &loans) {
    auto out = loans
      | genex::views::transform([](auto const &x) {
        return MakePair(KeyOf(x), Tup<bool, bool, bool>(x.Maybe.has_value(), x.Conflict.has_value(), x.Escaped));
      })
      | genex::to<Vec>();
    genex::actions::sort(out);
    return out;
  };

  // Check if the keys match.
  return keys(lhs) == keys(rhs);
}

/// [CHECKED]
auto spp::analyse::utils::memory_state::SettleFullyTaken(
  MemoryState state, VariableSymbol const &sym,
  Scope const &scope) -> MemoryState {
  // If the state is not marked as initialized, or the partial
  // moves are empty, it is settled so return the state as it
  // is.
  if (state.AstInitialization == nullptr
    or state.PartialMoves.empty()
    or sym.Type == nullptr) {
    return state;
  }

  // If the type is copyable, it can't be moved anyway, so
  // return the state as it is too.
  const auto type = TypeRef::Of(*sym.Type, scope);
  if (type.Symbol == nullptr or type.Symbol->IsCopyable()) {
    return state;
  }

  // Build a "place" for the provided symbol, with no steps.
  // This represents the entire symbol. If this place has
  // been consumed, then the state is settled.
  const auto whole = regions::Place{.Root = const_cast<VariableSymbol*>(&sym), .Steps = {}};
  if (not regions::RegionConsumed(whole, type, scope, state.PartialMoves)) { return state; }

  // Otherwise, set the state to "moved" (using the first
  // partial move as the site), clear the partial moves,
  // and reset the initialisation.
  state.AstMoved = state.PartialMoves.front().Site;
  state.AstInitialization = nullptr;
  state.PartialMoves.clear();
  return state;
}
