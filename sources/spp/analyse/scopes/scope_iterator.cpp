module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope;


SPP_MOD_BEGIN
spp::analyse::scopes::ScopeIterator::ScopeIterator(
    Scope *root) {
    if (root != nullptr) {
        _Stack.EmplaceBack(root, 0);
    }
}

auto spp::analyse::scopes::ScopeIterator::operator*()
    -> reference {
    return _Stack.Back().Node;
}

auto spp::analyse::scopes::ScopeIterator::operator*() const
    -> const_reference {
    return _Stack.Back().Node;
}

auto spp::analyse::scopes::ScopeIterator::operator->()
    -> pointer {
    return &_Stack.Back().Node;
}

auto spp::analyse::scopes::ScopeIterator::operator->() const
    -> const_pointer {
    return &_Stack.Back().Node;
}

auto spp::analyse::scopes::ScopeIterator::operator++()
    -> ScopeIterator& {
    // Nothing in the stack means that no more iteration can be done.
    if (_Stack.IsEmpty()) { return *this; }

    // Descend into unseen children of this node.
    auto [node, idx] = _Stack.Back();
    if (idx < node->Children.Len()) {
        _Stack.Back().Seen += 1;
        _Stack.EmplaceBack(node->Children[idx].Get(), 0);
        return *this;
    }

    // Otherwise, pop the stack to move up a level.
    _Stack.PopBack();

    // Walk upwards until a parent with unseen children is found.
    while (not _Stack.IsEmpty()) {
        auto &top = _Stack.Back();
        if (top.Seen < top.Node->Children.Len()) {
            top.Seen += 1;
            _Stack.EmplaceBack(top.Node->Children[top.Seen - 1].Get(), 0);
            return *this;
        }
        _Stack.PopBack();
    }

    // At this point, the stack is empty, meaning the iteration is complete.
    return *this;
}

auto spp::analyse::scopes::ScopeIterator::operator++(int)
    -> ScopeIterator {
    auto tmp = *this;
    ++*this;
    return tmp;
}

auto spp::analyse::scopes::ScopeIterator::operator==(
    ScopeIterator const &other) const
    -> bool {
    return _Stack.IsEmpty() and other._Stack.IsEmpty();
}

auto spp::analyse::scopes::ScopeIterator::operator!=(
    ScopeIterator const &other) const
    -> bool {
    return not(*this == other);
}

SPP_MOD_END
