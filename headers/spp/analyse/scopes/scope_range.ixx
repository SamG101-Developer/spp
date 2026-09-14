module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_range;
import spp.analyse.scopes.scope_iterator;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeRange);

/// The scope range is a type that exposes "begin" and "end"
/// method to expose the scope iterator type. Allows the C++
/// range-based "for" loops to work.
SPP_EXP_CLS class spp::analyse::scopes::ScopeRange {
  /// The root scope to begin iterating from. This is typically
  /// the current scope of the manager when the iteration is
  /// created.
  Scope *_Root;

public:
  /// Construct the scope range from the root scope to iterate
  /// from (sets the root field).
  explicit ScopeRange(Scope *root);

  /// Create the scope iterator using the root scope,
  /// incrementable using the "++" operator.
  SPP_ATTR_NODISCARD auto begin() const -> ScopeIterator;

  /// Create the scope iterator sentinel value by providing a
  /// nullptr scope.
  SPP_ATTR_NODISCARD auto end() const -> ScopeIterator;
};
