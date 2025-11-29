module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PrimaryExpressionAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
    PrimaryExpressionAst() = default;

    ~PrimaryExpressionAst() override;
};
