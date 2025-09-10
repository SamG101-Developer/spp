#pragma once
#include <spp/asts/statement_ast.hpp>


/**
 * The ExpressionAst class is the base class for all expressions in the abstract syntax tree. It inherits from
 * StatementAst, allowing it to be used in places where a statement is expected, but also providing additional
 * functionality specific to expressions.
 *
 * @n
 * Other base classes inherit this class, such as the PrimaryExpressionAst, which represents the most basic form of
 * expressions. Other expression such as unary, postfix, and binary expressions also inherit this class.
 *
 * @note This class is more of a "marker" to see where expressions should be, as there is no additional functionality in
 * this class.
 */
struct spp::asts::ExpressionAst : StatementAst {
    using StatementAst::StatementAst;
    friend struct ArrayLiteralExplicitElementsAst;
    friend struct ArrayLiteralRepeatedElementAst;
    friend struct BooleanLiteralAst;
    friend struct FloatLiteralAst;
    friend struct IdentifierAst;
    friend struct IntegerLiteralAst;
    friend struct StringLiteralAst;
    friend struct TupleLiteralAst;
    friend struct TypeIdentifierAst;
    friend struct TypeUnaryExpressionAst;
    friend struct TypePostfixExpressionAst;

    friend auto operator==(ExpressionAst const &lhs_expr, ExpressionAst const &rhs_expr) -> bool {
        return lhs_expr.equals(rhs_expr);
    }

protected:
    virtual auto equals_array_literal_explicit_elements(ArrayLiteralExplicitElementsAst const &) const -> bool;
    virtual auto equals_array_literal_repeated_elements(ArrayLiteralRepeatedElementAst const &) const -> bool;
    virtual auto equals_boolean_literal(BooleanLiteralAst const &) const -> bool;
    virtual auto equals_float_literal(FloatLiteralAst const &) const -> bool;
    virtual auto equals_identifier(IdentifierAst const &) const -> bool;
    virtual auto equals_integer_literal(IntegerLiteralAst const &) const -> bool;
    virtual auto equals_string_literal(StringLiteralAst const &) const -> bool;
    virtual auto equals_tuple_literal(TupleLiteralAst const &) const -> bool;
    virtual auto equals_type_identifier(TypeIdentifierAst const &) const -> bool;
    virtual auto equals_type_unary_expression(TypeUnaryExpressionAst const &) const -> bool;
    virtual auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> bool;
    virtual auto equals(ExpressionAst const &other) const -> bool;

public:
    ExpressionAst(ExpressionAst const &other);
};


#define ENFORCE_EXPRESSION_SUBTYPE(ast)                                                                    \
    if ((ast_cast<TypeAst>(ast) != nullptr) or (ast_cast<TokenAst>(ast) != nullptr)) {                     \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>().with_args( \
            *ast).with_scopes({sm->current_scope}).raise();                                                \
    }


#define ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(ast)                                                        \
    if (ast_cast<TypeAst>(ast) != nullptr) {                                                               \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>().with_args( \
            *ast).with_scopes({sm->current_scope}).raise();                                                \
    }


#define ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TYPE(ast)                                                         \
    if (ast_cast<TokenAst>(ast) != nullptr) {                                                              \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>().with_args( \
            *ast).with_scopes({sm->current_scope}).raise();                                                \
    }


#define RETURN_TYPE_OVERLOAD_HELPER(expr) \
    if (auto pe = ast_cast<PostfixExpressionAst>(expr); pe != nullptr and ast_cast<PostfixExpressionOperatorFunctionCallAst>(pe->op.get()) != nullptr)
