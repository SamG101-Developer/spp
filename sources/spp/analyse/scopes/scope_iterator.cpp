module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope;
import gch;

SPP_MOD_BEGIN
/// [CHECKED]
ScopeIterator::ScopeIterator(
  Scope *root) {
  if (root != nullptr) {
    _Stack.emplace_back(root, 0);
  }
}

/// [CHECKED]
ScopeIterator::ScopeIterator(ScopeIterator const &other) = default;

ScopeIterator::ScopeIterator(ScopeIterator &&other) noexcept = default;

auto ScopeIterator::operator=(ScopeIterator const &other) -> ScopeIterator& = default;

auto ScopeIterator::operator=(ScopeIterator &&other) noexcept -> ScopeIterator& = default;

ScopeIterator::~ScopeIterator() = default;

auto ScopeIterator::operator*() -> reference {
  // Hook into the node on the frame at the back of the stack.
  return _Stack.back().Node;
}

/// [CHECKED]
auto ScopeIterator::operator++() -> ScopeIterator& {
  // Nothing in the stack means that no more iteration can be
  // done.
  if (_Stack.empty()) { return *this; }

  // Descend into unseen children of this node.
  auto [node, idx] = _Stack.back();
  if (idx < node->Children.size()) {
    _Stack.back().Seen += 1;
    _Stack.emplace_back(node->Children[idx].get(), 0);
    return *this;
  }

  // Otherwise, pop the stack to move up a level.
  _Stack.pop_back();

  // Walk upwards until a parent with unseen children is found.
  while (not _Stack.empty()) {
    auto &top = _Stack.back();
    if (top.Seen < top.Node->Children.size()) {
      top.Seen += 1;
      _Stack.emplace_back(top.Node->Children[top.Seen - 1].get(), 0);
      return *this;
    }
    _Stack.pop_back();
  }

  // At this point, the stack is empty, meaning the iteration is
  // complete.
  return *this;
}

/// [CHECKED]
auto ScopeIterator::operator==(
  ScopeIterator const &other) const -> bool {
  return _Stack.empty() and other._Stack.empty();
}

/// [CHECKED]
auto ScopeIterator::operator!=(
  ScopeIterator const &other) const -> bool {
  return not(*this == other);
}

SPP_MOD_END
