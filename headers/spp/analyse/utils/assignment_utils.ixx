module;
#include <spp/macros.hpp>

export module spp.analyse.utils.assignment_utils;
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);

namespace spp::analyse::utils::assignment_utils {
  /// Check if an ast is an identifier ast type. This is used
  /// to determine which mutability and memory rules to apply
  /// during assignment analysis.
  SPP_EXP_FUN auto IsIdentifier(Ast const *expr) -> bool;

  /// Check if an ast is an attribute access ast type. This
  /// has alternative mutability/memory rules to enforce, and
  /// because it is only used from the assignment ast, we can
  /// just negate the identifier check.
  SPP_EXP_FUN auto IsAttr(Ast const *expr, ScopeManager const *sm) -> bool;

  /// Check if an ast is a dereference operation ast, by
  /// checking for a postfix expression ast and the operation
  /// bound to that ast.
  SPP_EXP_FUN auto IsDeref(Ast const *expr) -> bool;
}
