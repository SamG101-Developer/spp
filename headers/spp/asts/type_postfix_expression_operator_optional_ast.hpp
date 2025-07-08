#ifndef TYPE_POSTFIX_EXPRESSION_OPERATOR_OPTIONAL_AST_HPP
#define TYPE_POSTFIX_EXPRESSION_OPERATOR_OPTIONAL_AST_HPP

#include <spp/asts/type_postfix_expression_operator_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypePostfixExpressionOperatorOptionalAst final : TypePostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c ? token that represents the optional postfix operation. This indicates that the type will be moved into an
     * @code std::option::Opt[T]@endcode type, where @c T is the type of the left-hand side expression.
     */
    std::unique_ptr<TokenAst> tok_qst;

    /**
     * Construct the TypePostfixExpressionOperatorOptionalAst with the arguments matching the members.
     * @param tok_op The @c ? token that represents the optional postfix operation.
     */
    explicit TypePostfixExpressionOperatorOptionalAst(decltype(tok_qst) &&tok_op);
};


#endif //TYPE_POSTFIX_EXPRESSION_OPERATOR_OPTIONAL_AST_HPP
