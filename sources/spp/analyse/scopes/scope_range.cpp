#include <spp/analyse/scopes/scope_iterator.hpp>
#include <spp/analyse/scopes/scope_range.hpp>


spp::analyse::scopes::ScopeRange::ScopeRange(
    Scope *root) :
    m_root(root) {
}


auto spp::analyse::scopes::ScopeRange::begin() const
    -> ScopeIterator {
    return ScopeIterator(m_root);
}


auto spp::analyse::scopes::ScopeRange::end() const
    -> ScopeIterator {
    return ScopeIterator();
}
