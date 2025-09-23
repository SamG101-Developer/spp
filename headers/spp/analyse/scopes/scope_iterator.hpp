#pragma once
#include <vector>


namespace spp::analyse::scopes {
    class Scope;
    class ScopeIterator;
}


class spp::analyse::scopes::ScopeIterator {
    Scope *m_root;
    std::vector<Scope*> m_stack;
    std::size_t m_seen_children;

public:
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;
    using value_type = Scope*;
    using difference_type = std::ptrdiff_t;
    using pointer = Scope**;
    using reference = Scope*&;
    using const_pointer = Scope* const*;
    using const_reference = Scope* const&;

    explicit ScopeIterator(Scope *root = nullptr);
    auto operator*() -> reference;
    auto operator*() const -> const_reference;
    auto operator->() -> pointer;
    auto operator->() const -> const_pointer;
    auto operator++() -> ScopeIterator&;
    auto operator++(int) -> ScopeIterator;
    auto operator==(ScopeIterator const &other) const -> bool;
    auto operator!=(ScopeIterator const &other) const -> bool;
};
