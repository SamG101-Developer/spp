module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : Ast {
    PostfixExpressionOperatorAst();
    ~PostfixExpressionOperatorAst() override;
};
