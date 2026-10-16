module;
#include <spp/macros.hpp>

export module spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::utils::memory_state, struct BodyFlow);
use(spp::analyse::utils::memory_state, struct DeferExit);
use(spp::analyse::utils::memory_state, struct FlowState);
use(spp::analyse::utils::memory_state, struct KeptTemporary);
use(spp::analyse::utils::memory_state, struct LoopFrame);
use(spp::analyse::utils::memory_state, struct MemoryInfo);
use(spp::analyse::utils::memory_state, struct MemoryState);
use(spp::analyse::utils::memory_state, struct ScopeSnapshot);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);

namespace spp::analyse::utils::memory_state {
  /// Where paths that met disagreed about one aspect of a
  /// symbol: the path whose state was carried on, and one
  /// that differed from it. An error about the disagreement
  /// names both. Covers "maybe init", "maybe moved", etc.
  SPP_EXP_CLS struct PathsDisagree {
    Ast *First = nullptr;
    Ast *Other = nullptr;
  };

  /// A part moved off a symbol on some of the paths that
  /// met, and not on others.
  SPP_EXP_CLS struct MaybeMovedPart {
    regions::PlaceAt Part;
    PathsDisagree Paths;
  };

  /// What was done to a loan's root (or a place overlapping it)
  /// while the loan was held, which is an error only if the
  /// holder is used again: the root moved or written over, or
  /// borrowed or read where the loan allows neither. "Second"
  /// is the conflicting use.
  SPP_EXP_CLS struct LoanConflict {
    bool Moved = false;
    Ast const *First = nullptr;
    Ast const *Second = nullptr;
  };

  /// A borrow held past the expression that made it: by a
  /// coroutine or future handle given it as an argument, a
  /// closure capturing it, or the binding a yielded or indexed
  /// borrow is given. While it is held, the place cannot be
  /// moved, and only "&" borrows can join a "&" one. It ends
  /// when its holder does: the holder's scope ends, it is
  /// given a new value, or an "exit" leaves its scope.
  SPP_EXP_CLS struct Loan {
    regions::PlaceAt Borrowed;
    bool IsMut = false;
    VariableSymbol *Holder = nullptr;

    /// A "&mut" a generator yielded (the generator is the place
    /// borrowed): the generator's next resume ends it, as the
    /// next yield may be the same element.
    bool Yielded = false;

    /// Held on only some of the paths that met, and which. So
    /// when one path creates the escaping borrow and another
    /// doesn't, leaving an inconsistent state.
    Opt<PathsDisagree> Maybe;

    /// Made for a binding still being declared ("let h = c(&x)"):
    /// until it is ("ActivateLoans"), the expression making it is
    /// what holds the borrow, so no check sees it.
    bool Pending = false;

    /// A conflict met while it was held by a holder that acts
    /// only when used, waiting on the holder's next use.
    Opt<LoanConflict> Conflict;

    /// The holder, a borrow binding, was passed on somewhere that
    /// is not tracked (a block's or a branch's result): whatever
    /// now carries the borrow can be used without its holder
    /// being, so a conflict with it is raised at once. Handing it
    /// on by name ("TransferLoans") tracks it again.
    bool Escaped = false;
  };

  /// Every loan held in one function (or closure) body ("FlowState").
  SPP_EXP_CLS using LoanSet = Vec<Loan>;
}

/// The part of a symbol's memory information that branch
/// analysis saves and restores - what initialized/moved it,
/// partial moves, borrows etc. Where paths meet (the branches of
/// a "case", the ways out of a loop) their states are joined:
/// what they agree on is kept, and what only some of them did
/// is a "maybe", holding the paths that disagreed.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::MemoryState {
  /// The ast that initialized this value. Moving the value
  /// clears it, so the initialization and moved state are
  /// mutually exclusive.
  Ast const *AstInitialization = nullptr;

  /// The ast that moved this value. Initialising the value
  /// clears it, so the moved and initialization state are
  /// mutually exclusive.
  Ast const *AstMoved = nullptr;

  /// The value is held on some of the paths that met and not on
  /// others (initialized on one, moved or never given a value on
  /// another). "AstInitialization" and "AstMoved" are the first
  /// path's.
  Opt<PathsDisagree> MaybeInit;

  /// What the symbol still holds on every path is "Copy", so
  /// it is owed to nobody: a "case" subject narrowed to "None",
  /// or left to an "else" once every other member was matched.
  /// It can still be read; it never has to be consumed.
  bool OwesNothing = false;

  /// The list of partial moves taken from the symbol that
  /// this memory info is attached to, on every path. For the
  /// symbol "a", the partial move "a.b" would be in this list.
  Vec<regions::PlaceAt> PartialMoves;

  /// The parts taken on only some of the paths that met. Wrapped
  /// to include the "part" and the path disagreement data.
  Vec<MaybeMovedPart> MaybePartialMoves;

  /// A value has been given to the symbol, on some path, once
  /// or more, since it was declared (an immutable one cannot
  /// be given more than once), whether or not it still holds
  /// it. A persistent version of the "has this been initialised?".
  bool Given = false;

  /// This is the same as the initialisation marker, but
  /// doesn't get set to nullptr on move. This is used to
  /// effectively track the origin of the symbol when it
  /// was first ever initialised (usually the let/param
  /// declaration).
  Ast const *AstInitializationOrigin = nullptr;

  /// Set the initialisation marker, reset the moved marker,
  /// clear the partial move list, and mark the value given.
  auto InitializedBy(Ast const &ast) -> void;

  /// Set the moved marker, reset the initialisation marker. A
  /// whole move also takes every part, so the partial moves go.
  auto MovedBy(Ast const &ast) -> void;

  /// Record "part" taken off the value on this path (a partial
  /// move); any maybe-taken part inside it is settled.
  auto PartMovedBy(regions::PlaceAt const &part) -> void;

  /// Remove a specific partial move from the partial move list.
  /// This is used when part of the object is re-assigned. Once the
  /// vector is empty, no part can be inconsistently moved either.
  auto RemovePartialMoves(
    Ast const &ast, Scope const *scope) -> void;
};

/// What a symbol carries about its memory: its state as declared
/// (what Stage 7, or a walk outside any body, left it in), which a
/// body's walk starts each symbol's own state from ("StateOf"),
/// and what is fixed where it is declared.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::MemoryInfo : MemoryState {
  /// Where this symbol was declared as a borrow (its convention,
  /// or the type naming one); null for a symbol that owns its
  /// value. How long what it borrows lives is a loan's question
  /// ("borrows::EnforceBorrowsOutliveHolder"), not this.
  Ast const *AstBorrowed = nullptr;

  /// Clone the entire memory information struct for when a symbol
  /// is cloned (returns a unique pointer).
  SPP_ATTR_NODISCARD auto Clone() const -> Unique<MemoryInfo>;
};

/// A saved state, put back by "RestoreSnapshot": inside a body, a
/// copy of every symbol state and loan in its flow (not its loop
/// and "defer" bookkeeping, which belongs to the walk, not to a
/// path); outside one, the states of the symbols it was taken of
/// (held, so one replaced in its scope meanwhile is still the one
/// restored). "Symbols" lists those it was taken of, with their
/// states, either way.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::ScopeSnapshot {
  Vec<Pair<Shared<VariableSymbol>, MemoryState>> Symbols;
  FlowState *In = nullptr;
  StableMap<VariableSymbol const*, MemoryState> States;
  LoanSet Loans;
};

/// A loop being checked: the "skip"s that continue it (each a way
/// into another iteration besides the body's end), what a plain
/// "skip" left moved (those paths begin the next iteration too),
/// and the state at each "exit" that leaves it ("exit exit" leaves
/// the loop two back), which the loop joins into the state after
/// it.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::LoopFrame {
  std::size_t Skips = 0;
  Vec<Pair<VariableSymbol*, Ast const*>> SkipMoves;
  Vec<Pair<Ast const*, ScopeSnapshot>> ExitStates;
};

/// A "defer" being checked at an exit it runs from
/// ("DeferStatementAst::CheckAtExit"): the statement, and the exit.
/// A value its expression uses that is gone by then is reported
/// against both.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::DeferExit {
  Ast const *Stmt = nullptr;
  Ast const *ExitPoint = nullptr;
  StrView ExitWhat;
};

/// A linear temporary a "case" branch keeps (the "case" is over it
/// and the branch does not take it) in a branch that never finishes.
/// Nothing owns it once the walk leaves the branch's scope, so an
/// edge that does ("ret", "exit", "skip") drops it; one that never
/// leaves (an abort) does not.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::KeptTemporary {
  ExpressionAst *Temporary = nullptr;
  Ast const *Branch = nullptr;
};

/// The memory state of one function (or closure) body as its
/// walk has it: each symbol it has touched, by identity, and
/// the loans held. It lives on the body's outermost scope
/// while Stage 8 walks the body ("BodyFlow"); a path is a copy
/// of it, and paths meet by joining copies. A symbol is entered
/// on first touch, from its state as declared, so a holder or
/// value hidden by a shadowing "let" keeps its state, and
/// nothing a walk does is left on the symbol.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::FlowState {
  /// Each symbol's state by identity.
  StableMap<VariableSymbol const*, MemoryState> States;

  /// Every live borrow and who holds it.
  LoanSet Loans;

  /// The loops being checked in the body, innermost last.
  Vec<LoopFrame> Loops;

  /// The "defer" being checked at an exit, if one is.
  Opt<DeferExit> Deferring;

  /// The temporaries kept by the "case" branches being checked.
  Vec<KeptTemporary> KeptTemporaries;
};

/// An RAII guard that sets the flow in a scope to a new flow
/// state object, and replaces it again on destruction. This
/// is used whilst stage 8 does its walk.
SPP_EXP_CLS struct spp::analyse::utils::memory_state::BodyFlow {
  explicit BodyFlow(Scope const &body);
  ~BodyFlow();
  BodyFlow(BodyFlow const &) = delete;
  auto operator=(BodyFlow const &) -> BodyFlow& = delete;

private:
  Scope const *_Body;
  Shared<FlowState> _Outer;
};

namespace spp::analyse::utils::memory_state {
  /// Join "other", a symbol's state on the path "other_path", into
  /// "into", its state on "into_path", the path carried on. What
  /// the two agree on stays; what only one of them did becomes a
  /// "maybe" naming both paths. Nothing is raised here: a later
  /// use of the symbol throws an error.
  SPP_EXP_FUN auto Join(
    MemoryState &into, MemoryState const &other, Ast *into_path,
    Ast *other_path) -> void;

  /// A state of "sym" with every owned part taken off it, settled
  /// into a whole move (the first part's move as the witness): a
  /// value fully taken apart is consumed, so a path that did that
  /// and a path that moved it whole agree where they meet. "scope"
  /// is where the symbol's type resolves.
  SPP_EXP_FUN auto SettleFullyTaken(
    MemoryState state, VariableSymbol const &sym,
    Scope const &scope) -> MemoryState;

  /// Whether two states of a symbol agree on everything a later
  /// use is checked against: held or not, the parts taken (each
  /// by place), the maybes. Where they were written is not
  /// compared, so two passes over a loop body that did the same
  /// thing agree.
  SPP_EXP_FUN auto SameShape(
    MemoryState const &lhs, MemoryState const &rhs) -> bool;

  /// A loan of "borrowed", held by "holder" (none for one the call
  /// being checked holds while it runs).
  SPP_EXP_FUN auto MakeLoan(
    regions::PlaceAt borrowed, bool is_mut, VariableSymbol *holder = nullptr,
    bool yielded = false, bool pending = false) -> Loan;

  /// The flow of the body "scope" is in: the one on the nearest
  /// enclosing scope that has one ("BodyFlow"), if any.
  SPP_EXP_FUN auto FlowOf(
    Scope const &scope) -> FlowState*;

  /// The memory state of "sym" where "scope" is: its entry in the
  /// body's flow (made from its state as declared on first touch),
  /// or, outside any body, the symbol's own.
  SPP_EXP_FUN auto StateOf(
    Scope const &scope, VariableSymbol const &sym) -> MemoryState&;

  /// The state of "sym" in "snapshot".
  SPP_EXP_FUN auto StateIn(
    ScopeSnapshot const &snapshot, VariableSymbol const &sym) -> MemoryState;

  /// The loans held in the body "scope" is in (outside any body,
  /// a set of their own).
  SPP_EXP_FUN auto LoansOf(
    Scope const &scope) -> LoanSet&;

  /// The loans "holder" holds, now that it is declared: no check
  /// saw them while its "let" was still being checked.
  SPP_EXP_FUN auto ActivateLoans(
    LoanSet &loans, VariableSymbol const &holder) -> void;

  /// The loans "holder" holds (none still pending).
  SPP_EXP_FUN auto LoansHeldBy(
    LoanSet const &loans, VariableSymbol const &holder) -> Vec<Loan const*>;

  /// End every loan "holder" holds, as it ends (its scope ends, it
  /// is given a new value, an "exit" leaves its scope).
  SPP_EXP_FUN auto ReleaseLoans(
    LoanSet &loans, VariableSymbol const &holder) -> void;

  /// Hand every loan "from" holds to "to" (a handle moved into
  /// another name, a yielded borrow moved into another binding, a
  /// future's borrows to the generator "await" produces).
  SPP_EXP_FUN auto TransferLoans(
    LoanSet &loans, VariableSymbol const &from, VariableSymbol &to,
    bool pending = false) -> void;

  /// Join "other", the loans held on the path "other_path", into
  /// "into", those on "into_path": a loan held on either is held,
  /// and one held on only one of them is a maybe naming both.
  SPP_EXP_FUN auto JoinLoans(
    LoanSet &into, LoanSet const &other, Ast *into_path,
    Ast *other_path) -> void;

  /// Whether two sets hold the same loans, maybes included.
  SPP_EXP_FUN auto SameLoans(
    LoanSet const &lhs, LoanSet const &rhs) -> bool;

  /// The current memory state of each of "syms", and the loans
  /// held in the body "scope" is in.
  SPP_EXP_FUN auto SnapshotSymbols(
    Vec<VariableSymbol*> const &syms,
    Scope const &scope)
    -> ScopeSnapshot;

  /// The whole state of the body "scope" is in, symbols and loans,
  /// for an edge to change and put back (an "exit", a "defer", the
  /// scope ends a "ret" passes): every symbol the walk has touched,
  /// whatever its scope and whether or not it is shadowed (outside
  /// any body, every symbol visible from "scope").
  SPP_EXP_FUN auto SnapshotFlow(
    Scope const &scope)
    -> ScopeSnapshot;

  /// Put every symbol in "snapshot", and its loans, back into
  /// their saved state.
  SPP_EXP_FUN auto RestoreSnapshot(
    ScopeSnapshot const &snapshot)
    -> void;
}
