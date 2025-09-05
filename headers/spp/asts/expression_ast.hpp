#pragma once
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


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
