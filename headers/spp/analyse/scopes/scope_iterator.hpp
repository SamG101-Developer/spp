#pragma once
#include <vector>


/// @cond
namespace spp::analyse::scopes {
    class Scope;
    class ScopeIterator;
}
/// @endcond



/**
 * The @c ScopeIterator is an iterator that performs a depth-first traversal of the scope tree, starting from the given
 * root scope. It visits the root scope first, then recursively visits each child scope in order. The iterator is
 * compatible with STL algorithms.
 */
class spp::analyse::scopes::ScopeIterator {
    /**
     * The root scope to start the traversal from. This is given in the constructor. If the @c root is @c nullptr, then
     * this is an <i>end</i> iterator or <i>sentinel</i>.
     */
    Scope *m_root;

    /**
     * The stack that the iterator maintains to keep track of the current scope and its ancestors. The top of the stack
     * is the current scope.
     */
    std::vector<Scope*> m_stack;

    /**
     * The number of child scopes that have been seen so far. This is used to determine when to pop the stack and move
     * back up the tree.
     */
    std::size_t m_seen_children;

public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
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
