module;
#include <spp/macros.hpp>

export module spp.analyse.utils.assignment_utils;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
}


namespace spp::analyse::utils::assignment_utils {
    /**
     * Check if an AST is of the @c IdentifierAst type. This is put as a function here for uniform comparison to whether
     * an AST is an attribute or not.
     * @param expr The AST to check.
     * @return If the AST is an identifier, return true. Otherwise, return false.
     */
    SPP_EXP_FUN auto is_identifier(
        asts::Ast const *expr)
        -> bool;

    /**
     * Check if an AST is an attribute, by checking for the @c PostfixExpressionAst, with an operator whose type is
     * @c PostfixExpressionOperatorRuntimeAccessAst. However, because this is only used from the
     * @c AssignmentExpressionAst, we can just negate the identifier check.
     * @param expr The AST to check.
     * @param sm
     * @return
     */
    SPP_EXP_FUN auto is_attr(
        asts::Ast const *expr,
        scopes::ScopeManager const *sm)
        -> bool;
}
