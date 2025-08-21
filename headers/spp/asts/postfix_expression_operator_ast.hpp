#ifndef POSTFIX_EXPRESSION_OPERATOR_AST_HPP
#define POSTFIX_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/_fwd.hpp>
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/type_inferrable.hpp>


struct spp::asts::PostfixExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;
};


#endif //POSTFIX_EXPRESSION_OPERATOR_AST_HPP
