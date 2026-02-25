module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantElseCaseAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantElseCaseAst final : CasePatternVariantAst {
    std::unique_ptr<TokenAst> tok_else;
    std::unique_ptr<CaseExpressionAst> case_expr;

    explicit CasePatternVariantElseCaseAst(
        decltype(tok_else) &&tok_else,
        decltype(case_expr) &&case_expr);
    ~CasePatternVariantElseCaseAst() override;
    auto to_rust() const -> std::string override;
};
