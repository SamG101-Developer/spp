module spp.analyse.scopes.scope_range;
import spp.analyse.scopes.scope;


spp::analyse::scopes::ScopeRange::ScopeRange(
    Scope *root) :
    m_root(root) {
}


auto spp::analyse::scopes::ScopeRange::begin() const
    -> ScopeIterator {
    return ScopeIterator(m_root);
}


auto spp::analyse::scopes::ScopeRange::end() const // NOLINT(readability-convert-member-functions-to-static)
    -> ScopeIterator {
    return ScopeIterator();
}
