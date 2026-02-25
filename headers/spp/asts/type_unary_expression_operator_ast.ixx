module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorAst : Ast {
    explicit TypeUnaryExpressionOperatorAst();
    ~TypeUnaryExpressionOperatorAst() override;
};
