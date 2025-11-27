module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;

    ~PostfixExpressionOperatorAst() override;
};
