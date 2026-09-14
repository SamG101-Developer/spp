module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_info_utils;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::analyse::utils::mem_info_utils, struct MemoryState);
use(spp::analyse::utils::mem_info_utils, struct MemoryConsistency);
use(spp::analyse::utils::mem_info_utils, struct MemoryInfo);

namespace spp::analyse::utils::mem_info_utils {
  /// Two conflicting branches in terms of memory integrity
  /// of reported symbols.
  SPP_EXP_CLS using InconsistentCondMemState = Pair<Ast*, Ast*>;

  /// A snapshot of the memory information of a symbol, used
  /// during branch analysis.
  SPP_EXP_CLS using MemoryInfoSnapshot = MemoryState;
}

/// The part of a symbol's memory information that branch
/// analysis saves and restores - what initialized/moved it,
/// partial moves, borrows etc.
SPP_EXP_CLS struct spp::analyse::utils::mem_info_utils::MemoryState {
  /// The ast that initialized this value, and the scope the
  /// initialized occurred in. Moving a value will set these
  /// to nullptr, so the initialization and moved state are
  /// mutually exclusive.
  Tup<Ast const*, Scope*> AstInitialization = {nullptr, nullptr};

  /// The ast that moved this value, and the scope that move
  /// occurred in. Initialising a value will set these to to
  /// nullptr, so the moved and initialization state are
  /// mutually exclusive.
  Tup<Ast const*, Scope*> AstMoved = {nullptr, nullptr};

  /// The list of partial moves taken from the symbol that
  /// this memory info is attached to. For the symbol "a",
  /// the partial move "a.b" would be in this list.
  Vec<Ast const*> AstPartialMoves;

  /// A list of the escaping borrows that this symbol contains,
  /// such as when this symbol is a coroutine or async handle,
  /// escaping the typical inner frame lifetime constraint.
  Vec<Tup<Ast const*, bool, Scope*>> AstContainedEscapingBorrows;

  /// A reverse map of the contained escaping borrows map;
  /// this is a list of what borrows this symbol, in an
  /// escaping manner.
  Vec<Tup<Ast const*, Ast const*>> AstContainersOfEscapingBorrows;

  /// The initialisation counter is the number of times the
  /// symbol has been initialised. This is used for "let"
  /// statements that aren't initialised on declaration,
  /// because they cans till receive a value despite being
  /// immutable.
  std::size_t InitializationCounter = 0;
};

/// How a symbol's memory state came out of the branches of
/// a "case" expression: each field records the pair of branches
/// that disagreed about one aspect of it such as the
/// initialisation state, and is empty when they all agreed.
SPP_EXP_CLS struct spp::analyse::utils::mem_info_utils::MemoryConsistency {
  /// When a symbol is uninitialised before the "case" expression
  /// is analysed, and one of the branches initialises it and
  /// another doesn't.
  std::optional<InconsistentCondMemState> IsInconsistentlyInitialized;

  /// When a symbol is initialised before the "case" expression
  /// is analysed, and one of the branches moves it and another
  /// doesn't.
  std::optional<InconsistentCondMemState> IsInconsistentlyMoved;

  /// When the vector of partial moves disagree at the end of
  /// the branch's analysis.
  std::optional<InconsistentCondMemState> IsInconsistentlyPartiallyMoved;

  /// When the vector of escaping borrows disagree at the end
  /// of the branch's analysis (either of the borrow vectors).
  std::optional<InconsistentCondMemState> IsInconsistentlyBorrowEscaping;
};

/// The MemoryInfo struct is used to track the memory state
/// of a symbol in the scope. It contains all the state and
/// consistency information, as-well as some additional
/// fields such as if this is a borrow (say a function param)
/// and a comptime field.
SPP_EXP_CLS struct spp::analyse::utils::mem_info_utils::MemoryInfo : MemoryState, MemoryConsistency {
  /// This is the same as the initialisation marker, but
  /// doesn't get set to nullptr on move. This is used to
  /// effectively track the origin of the symbol when it
  /// was first ever initialised (usually the let/param
  /// declaration).
  Tup<Ast const*, Scope*> AstInitializationOrigin = {nullptr, nullptr};

  /// Where this symbol was declared as a borrow - same
  /// structure as other markers, the location and scope. The
  /// scope is important here as it is used for lifetime
  /// checks. Nullptr => owned type onm the symbol.
  Tup<Ast const*, Scope*> AstBorrowed = {nullptr, nullptr};


  /// Set the initialisation marker, reset the moved marker,
  /// clear the partial move list, and increment the
  /// initialisation counter by 1.
  auto InitializedBy(Ast const &ast, Scope *scope) -> void;

  /// Set the moved marker, reset the initialisation marker.
  auto MovedBy(Ast const &ast, Scope *scope) -> void;

  /// Remove a specific partial move from the partial move list.
  /// This is used when part of the object is re-assigned. If the
  /// vector is now empty, then the object is marked as initialised.
  auto RemovePartialMoves(Ast const &ast, Scope *scope) -> void;

  /// Generate a copied snapshot of the memory state, used to capture
  /// state only for before/after analysis.
  SPP_ATTR_NODISCARD auto Snapshot() const -> MemoryInfoSnapshot;

  /// Clone the entire memory information struct for when a symbol
  /// is cloned (returns a unique pointer).
  SPP_ATTR_NODISCARD auto Clone() const -> Unique<MemoryInfo>;

  /// Restore the memory information from a snapshot, such as once
  /// the branch analysis has finished.
  auto FillFromSnapshot(MemoryInfoSnapshot const &snapshot) -> void;
};
