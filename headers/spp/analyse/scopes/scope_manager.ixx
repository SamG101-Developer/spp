module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope_tree;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct ScopePositionGuard);
use(spp::asts, struct Ast);
use(spp::utils::errors, class ErrorFormatter);

/// The scope manager type is the most critical structure of
/// the entire analysis engine. It contains the entire scope
/// tree of the code, providing the overarching structure,
/// and the symbols within those scopes. It also provides
/// scope iteration capability (depth-first).
SPP_EXP_CLS class spp::analyse::scopes::ScopeManager {
  /// The current iterator state. This is a customised
  /// iterator for depth-first searching, always positioned at
  /// the current scope when a manager is made.
  ScopeIterator _It;

  /// The tree this manager walks: one compile's scopes, shared by
  /// every manager over it.
  ScopeTree *_Tree;

  /// Over "tree", standing at "current_scope" (the global scope
  /// if null). Managers come from "ForTree" or another manager's
  /// "NewManagerAt".
  explicit ScopeManager(ScopeTree &tree, Scope *current_scope = nullptr);

public:
  /// The current scope is the scope that has been iterated
  /// to, and stays consistent with the ast it is being used
  /// for between stages of the compiler.
  Scope *CurrentScope = nullptr;

  /// The root manager of a compile, walking "tree" from its
  /// global scope.
  SPP_ATTR_NODISCARD static auto ForTree(ScopeTree &tree) -> ScopeManager;

  /// The tree this manager walks.
  SPP_ATTR_NODISCARD auto Tree() const -> ScopeTree&;

  /// The root of that tree.
  SPP_ATTR_NODISCARD auto GlobalScope() const -> Scope*;

  /// Default destructor logic.
  ~ScopeManager();

  /// A new manager standing at "scope", to read there or to walk
  /// its subtree: the first "MoveToNextScope" enters its first
  /// child. Positioning allocates nothing ("ScopeIterator").
  SPP_ATTR_NODISCARD auto NewManagerAt(Scope *scope) const -> ScopeManager;

  /// A new manager resuming a walk where "pos" stood (replaying
  /// a "defer", walking a loop body again).
  SPP_ATTR_NODISCARD auto NewManagerAt(ScopePosition const &pos) const -> ScopeManager;

  /// Where this manager's walk stands: its scope and iterator.
  SPP_ATTR_NODISCARD auto Position() const -> ScopePosition;

  /// Put this manager's walk back where "pos" stood. For a detour
  /// that has to come back, "ScopePositionGuard" does this on
  /// leaving; this is for a return made explicitly, from a catch.
  auto ReturnTo(ScopePosition const &pos) -> void;

  /// Start a whole walk again from "scope" (the global scope by
  /// default): each compiler stage begins with one.
  auto RestartWalk(Scope *scope = nullptr) -> void;

  /// Create a new scope and move into it. It will be the child
  /// of the current scope, and the internal iterator will
  /// update to point into it on iteration. Because of how the
  /// iterator works, "live updates" are fine and the next
  /// scope is this new one. The name of the scope is provided
  /// here, alongside an optional error formatter.
  auto CreateAndMoveIntoNewScope(
    ScopeName const &name,
    Ast *ast = nullptr,
    ErrorFormatter *error_formatter = nullptr)
    -> Scope*;

  /// Move out of the current scope and into the parent scope.
  /// This is used after a scope has been moved into for a
  /// function or class. Returns the parent scope too.
  auto MoveOutOfCurrentScope() -> Scope*;

  /// Iterate into the next scope without creating a new scope.
  /// This just increments the iterator, and skips alias class
  /// scopes so that each ast sync properly.
  auto MoveToNextScope() -> Scope*;

  /// Skip "scope"'s subtree: advance the walk to its final
  /// descendant ("GetFinalChildScope"), from wherever in the
  /// subtree it has got to, leaving the current scope on "scope"
  /// itself - so a following "MoveOutOfCurrentScope" reaches its
  /// parent, and one more "MoveToNextScope" its next sibling.
  /// False if the walk ran out first. A null "scope" (a member
  /// with no scope of its own) skips nothing.
  auto SkipSubtree(Scope *scope) -> bool;
};

/// A detour on a manager: stand anywhere, walk anywhere, and on
/// leaving - an error included - the manager is back where it
/// was. For analysing an expression built on the fly, or reading
/// from another scope on the same manager.
SPP_EXP_CLS struct spp::analyse::scopes::ScopePositionGuard {
  explicit ScopePositionGuard(ScopeManager &sm);
  ~ScopePositionGuard();

  ScopePositionGuard(ScopePositionGuard const &) = delete;
  auto operator=(ScopePositionGuard const &) -> ScopePositionGuard& = delete;

private:
  ScopeManager &_Sm;
  ScopePosition _Was;
};
