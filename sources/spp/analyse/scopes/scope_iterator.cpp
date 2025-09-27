#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_iterator.hpp>


spp::analyse::scopes::ScopeIterator::ScopeIterator(
    Scope *root) :
    m_root(root) {
    if (m_root != nullptr) {
        m_stack.push_back(m_root);
        m_seen_children = m_root->children.size();
    }
}


auto spp::analyse::scopes::ScopeIterator::operator*()
    -> reference {
    return m_stack.back();
}


auto spp::analyse::scopes::ScopeIterator::operator*() const
    -> const_reference {
    return m_stack.back();
}


auto spp::analyse::scopes::ScopeIterator::operator->()
    -> pointer {
    return &m_stack.back();
}


auto spp::analyse::scopes::ScopeIterator::operator->() const
    -> const_pointer {
    return &m_stack.back();
}


auto spp::analyse::scopes::ScopeIterator::operator++()
    -> ScopeIterator& {
    if (m_stack.empty()) { return *this; }

    const auto cur = m_stack.back();
    m_stack.pop_back();

    // Push children of the current scope.
    for (auto it = cur->children.rbegin(); it != cur->children.rend(); ++it) {
        m_stack.push_back(it->get());
    }

    // If exhausted, but root got new children, pick them up.
    if (m_stack.empty() and m_root != nullptr) {
        for (auto i = m_seen_children; i < m_root->children.size(); ++i) {
            m_stack.push_back(m_root->children[i].get());
        }
        m_seen_children = m_root->children.size();
    }

    // Return self.
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

