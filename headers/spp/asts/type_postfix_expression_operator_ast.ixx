module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypePostfixExpressionOperatorAst;
}


SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorAst : Ast {
    explicit TypePostfixExpressionOperatorAst();
    ~TypePostfixExpressionOperatorAst() override;
};
