#ifndef UNARY_EXPRESSION_OPERATOR_AST_HPP
#define UNARY_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::UnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
};


#endif //UNARY_EXPRESSION_OPERATOR_AST_HPP
