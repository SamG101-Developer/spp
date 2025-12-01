module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAst;
}


SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    UnaryExpressionOperatorAst() = default;

    ~UnaryExpressionOperatorAst() override;
};


spp::asts::UnaryExpressionOperatorAst::~UnaryExpressionOperatorAst() = default;
