module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PrimaryExpressionAst;
    SPP_EXP_CLS struct TypeAst;  // DO NOT REMOVE (causes module errors)
    SPP_EXP_CLS struct IdentifierAst;  // DO NOT REMOVE (causes module errors)
}


SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
    PrimaryExpressionAst() = default;

    ~PrimaryExpressionAst() override;
};
