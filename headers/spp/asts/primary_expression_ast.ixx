module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PrimaryExpressionAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
}


SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
    using ExpressionAst::ExpressionAst;

    ~PrimaryExpressionAst() override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
