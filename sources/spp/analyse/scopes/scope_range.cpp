module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_range;
import spp.analyse.scopes.scope;

SPP_MOD_BEGIN
ScopeRange::ScopeRange(Scope *root) :
  _Root(root) {
}

auto ScopeRange::begin() const -> ScopeIterator {
  return ScopeIterator(_Root);
}

auto ScopeRange::end() const -> ScopeIterator {
  return ScopeIterator();
}

SPP_MOD_END
