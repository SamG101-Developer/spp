module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantExpressionAst;
    SPP_EXP_CLS struct ExpressionAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
    std::unique_ptr<ExpressionAst> expr;

    explicit CasePatternVariantExpressionAst(
        decltype(expr) &&expr);
    ~CasePatternVariantExpressionAst() override;
    auto to_rust() const -> std::string override;
};
