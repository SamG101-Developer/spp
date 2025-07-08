#ifndef POSTFIX_EXPRESSION_OPERATOR_AST_HPP
#define POSTFIX_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::PostfixExpressionOperatorAst : ExpressionAst {
    using ExpressionAst::ExpressionAst;
};


#endif //POSTFIX_EXPRESSION_OPERATOR_AST_HPP
