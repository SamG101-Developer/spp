module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct UnaryExpressionOperatorAst;
}


SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAst : Ast {
    explicit UnaryExpressionOperatorAst() = default;
    ~UnaryExpressionOperatorAst() override;
};
