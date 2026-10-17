module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_tree;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct ScopeTree);
use(spp::analyse::scopes, struct TypeSymbol);

/// One compilation's scopes: the global scope, the root of the
/// tree, and the lists built while walking it. One per compile,
/// owned by the compiler; every scope manager over it points here
/// ("ScopeManager::Tree"). Destroying it ends the compile: the
/// caches elsewhere that are keyed by its scopes are cleared, and
/// then its scopes are freed.
SPP_EXP_CLS struct spp::analyse::scopes::ScopeTree {
  /// The root of the tree, which records this tree as its own
  /// ("Scope::Tree").
  Shared<Scope> Global;

  /// Each type's non-generic sup blocks, as they are loaded.
  Map<TypeSymbol*, Vec<Scope*>> NormalSupBlocks;

  /// The generic sup blocks ("sup [T] T { ... }"), which apply to
  /// every type they can.
  Vec<Scope*> GnSupBlocks;

  /// Scopes made for things that have no place in the tree of
  /// their own (a closure's, a generic parameter's dummy), held
  /// here for as long as the compile.
  Vec<Unique<Scope>> TempScopes;

  explicit ScopeTree(Shared<Scope> global);
  ~ScopeTree();

  ScopeTree(ScopeTree const &) = delete;
  auto operator=(ScopeTree const &) -> ScopeTree& = delete;
};
