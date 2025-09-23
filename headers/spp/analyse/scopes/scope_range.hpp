#pragma once
#include <spp/macros.hpp>


namespace spp::analyse::scopes {
    class Scope;
    class ScopeRange;
}


/**
 * A @c ScopeRange is a range type that can be used in range-based for loops to iterate the scopes in a @c ScopeManager.
 * It contains a @c begin and @c end function, separating them from the actual @c ScopeManager type.
 */
class spp::analyse::scopes::ScopeRange {
    /**
     * The root scope to begin iterating from. This is typically the current scope of the manager when the iteration is
     * created.
     */
    Scope *m_root;

public:
    /**
     * Construct the ScopeRange from the root scope to iterate from.
     * @param root The root to iterate from. This is set to the @c root attribute.
     */
    explicit ScopeRange(Scope *root);

    /**
     * The @c begin iterator for the range. This creates a new iterator starting from the root scope, and can be
     * incremented using the @code ++@endcode operator.
     * @return The @c begin iterator for the @c ScopeManager.
     */
    SPP_ATTR_NODISCARD auto begin() const -> ScopeIterator;

    /**
     * The @c end iterator for the range. This creates a new iterator with no root scope, which represents the end of
     * the iteration.
     * @return The @c end iterator for the @c ScopeManager.
     */
    SPP_ATTR_NODISCARD auto end() const -> ScopeIterator;
};
