module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope;

SPP_MOD_BEGIN
ScopeIterator::ScopeIterator(
  Scope *root) {
  if (root != nullptr) {
    _Stack.EmplaceBack(root, 0);
  }
}

auto ScopeIterator::operator*() -> reference {
  // Hook into the node on the frame at the back of the stack.
  return _Stack.Back().Node;
}

auto ScopeIterator::operator*() const -> const_reference {
  // Hook into the node on the frame at the back of the stack.
  return _Stack.Back().Node;
}

auto ScopeIterator::operator->() -> pointer {
  // Hook into the node on the frame at the back of the stack.
  return &_Stack.Back().Node;
}

auto ScopeIterator::operator->() const -> const_pointer {
  // Hook into the node on the frame at the back of the stack.
  return &_Stack.Back().Node;
}

auto ScopeIterator::operator++() -> ScopeIterator& {
  // Nothing in the stack means that no more iteration can be
  // done.
  if (_Stack.IsEmpty()) { return *this; }

  // Descend into unseen children of this node.
  auto [node, idx] = _Stack.Back();
  if (idx < node->Children.Len()) {
    _Stack.Back().Seen += 1;
    _Stack.EmplaceBack(node->Children[idx].get(), 0);
    return *this;
  }

  // Otherwise, pop the stack to move up a level.
  _Stack.PopBack();

  // Walk upwards until a parent with unseen children is found.
  while (not _Stack.IsEmpty()) {
    auto &top = _Stack.Back();
    if (top.Seen < top.Node->Children.Len()) {
      top.Seen += 1;
      _Stack.EmplaceBack(top.Node->Children[top.Seen - 1].get(), 0);
      return *this;
    }
    _Stack.PopBack();
  }

  // At this point, the stack is empty, meaning the iteration is
  // complete.
  return *this;
}

auto ScopeIterator::operator++(int) -> ScopeIterator {
  auto tmp = *this;
  ++*this;
  return tmp;
}

auto ScopeIterator::operator==(
  ScopeIterator const &other) const -> bool {
  return _Stack.IsEmpty() and other._Stack.IsEmpty();
}

auto ScopeIterator::operator!=(
  ScopeIterator const &other) const -> bool {
  return not(*this == other);
}

SPP_MOD_END
