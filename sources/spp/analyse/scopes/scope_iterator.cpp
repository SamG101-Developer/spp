module spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope;


spp::analyse::scopes::ScopeIterator::ScopeIterator(
    Scope *root) {
    if (root != nullptr) {
        m_stack.emplace_back(root, 0);
    }
}


auto spp::analyse::scopes::ScopeIterator::operator*()
    -> reference {
    return m_stack.back().node;
}


auto spp::analyse::scopes::ScopeIterator::operator*() const
    -> const_reference {
    return m_stack.back().node;
}


auto spp::analyse::scopes::ScopeIterator::operator->()
    -> pointer {
    return &m_stack.back().node;
}


auto spp::analyse::scopes::ScopeIterator::operator->() const
    -> const_pointer {
    return &m_stack.back().node;
}


auto spp::analyse::scopes::ScopeIterator::operator++()
    -> ScopeIterator& {
    // Nothing in the stack means that no more iteration can be done.
    if (m_stack.empty()) { return *this; }

    // Descend into unseen children of this node.
    auto [node, idx] = m_stack.back();
    if (idx < node->children.size()) {
        m_stack.back().seen += 1;
        m_stack.emplace_back(node->children[idx].get(), 0);
        return *this;
    }

    // Otherwise, pop the stack to move up a level.
    m_stack.pop_back();

    // Walk upwards until a parent with unseen children is found.
    while (not m_stack.empty()) {
        auto &top = m_stack.back();
        if (top.seen < top.node->children.size()) {
            top.seen += 1;
            m_stack.emplace_back(top.node->children[top.seen - 1].get(), 0);
            return *this;
        }
        m_stack.pop_back();
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
    return m_stack.empty() and other.m_stack.empty();
}


auto spp::analyse::scopes::ScopeIterator::operator!=(
    ScopeIterator const &other) const
    -> bool {
    return not(*this == other);
}
