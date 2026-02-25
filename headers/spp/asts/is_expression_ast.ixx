module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IsExpressionAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
}


SPP_EXP_CLS struct spp::asts::IsExpressionAst final : ExpressionAst {
    std::unique_ptr<ExpressionAst> lhs;
    std::unique_ptr<CasePatternVariantAst> rhs;

    IsExpressionAst(
        decltype(lhs) &&lhs,
        decltype(rhs) &&rhs);
    ~IsExpressionAst() override;
    auto to_rust() const -> std::string override;
};
