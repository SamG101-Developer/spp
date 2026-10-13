module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.symbols;
import std;

SPP_MOD_BEGIN
ScopeManager::ScopeManager(
  ScopeTree &tree,
  Scope *current_scope) :
  _Tree(&tree),
  CurrentScope(current_scope ? current_scope : tree.Global.get()) {
}

auto ScopeManager::ForTree(
  ScopeTree &tree) -> ScopeManager {
  auto out = ScopeManager(tree);
  out.RestartWalk();
  return out;
}

auto ScopeManager::Tree() const -> ScopeTree& {
  return *_Tree;
}

auto ScopeManager::GlobalScope() const -> Scope* {
  return _Tree->Global.get();
}

ScopeManager::~ScopeManager() = default;

auto ScopeManager::NewManagerAt(
  Scope *scope) const -> ScopeManager {
  auto out = ScopeManager(*_Tree, scope);
  out.RestartWalk(scope);
  return out;
}

auto ScopeManager::NewManagerAt(
  ScopePosition const &pos) const -> ScopeManager {
  auto out = ScopeManager(*_Tree, pos.At);
  out.ReturnTo(pos);
  return out;
}

auto ScopeManager::Position() const -> ScopePosition {
  return ScopePosition{.At = CurrentScope, .It = _It};
}

auto ScopeManager::ReturnTo(
  ScopePosition const &pos) -> void {
  CurrentScope = pos.At;
  _It = pos.It;
}

auto ScopeManager::RestartWalk(
  Scope *scope) -> void {
  // From the provided scope, or the global scope.
  CurrentScope = scope ? scope : GlobalScope();
  _It = ScopeIterator{CurrentScope};
}

auto ScopeManager::CreateAndMoveIntoNewScope(
  ScopeName const &name, Ast *ast, ErrorFormatter *error_formatter)
  -> Scope* {
  // Create a new scope, using the current scope as the parent
  // scope.
  auto scope = MakeUnique<Scope>(name, CurrentScope, ast, error_formatter);
  CurrentScope->Children.emplace_back(std::move(scope));
  ++_It;

  // Set the new scope as the current scope, and advance the
  // iterator to match.
  CurrentScope = CurrentScope->Children.back().get();
  return CurrentScope;
}

auto ScopeManager::MoveOutOfCurrentScope() -> Scope* {
  // Exit the current scope into the parent scope, which reads
  // as it did before the walk entered it.
  CurrentScope->RevealLetBindings();
  CurrentScope = CurrentScope->Parent;
  return CurrentScope;
}

auto ScopeManager::MoveToNextScope() -> Scope* {
  // Move to the next scope by advancing the iterator, past any
  // alias class scope (an alias's scope is walked by nothing).
  CurrentScope = *++_It;
  while (CurrentScope->LinkedTypeSymbol != nullptr and CurrentScope->LinkedTypeSymbol->Alias != nullptr) {
    CurrentScope = *++_It;
  }

  // A walk into a scope starts with none of its "let"s declared yet.
  CurrentScope->HideLetBindings();
  return CurrentScope;
}

auto ScopeManager::SkipSubtree(
  Scope *scope) -> bool {
  // A member with no scope of its own consumed none of the
  // walk, so there is nothing to skip.
  if (scope == nullptr) { return true; }

  // Advance from wherever in the subtree the walk has got to,
  // onto its final descendant, and stand on the scope skipped.
  const auto final_scope = scope->GetFinalChildScope();
  const auto end = ScopeIterator();
  while (_It != end and *_It != final_scope) { ++_It; }
  scope->RevealNestedLetBindings();
  CurrentScope = scope;
  return _It != end;
}

ScopePositionGuard::ScopePositionGuard(
  ScopeManager &sm) :
  _Sm(sm),
  _Was(sm.Position()) {
}

ScopePositionGuard::~ScopePositionGuard() {
  _Sm.ReturnTo(_Was);
}

SPP_MOD_END
