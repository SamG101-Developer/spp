#ifndef UNARY_EXPRESSION_OPERATOR_AST_HPP
#define UNARY_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/_fwd.hpp>
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/type_inferrable.hpp>


struct spp::asts::UnaryExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;
};


#endif //UNARY_EXPRESSION_OPERATOR_AST_HPP
