module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantLiteralAst;
    SPP_EXP_CLS struct LiteralAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
    std::unique_ptr<LiteralAst> literal;

    explicit CasePatternVariantLiteralAst(
        decltype(literal) &&literal);
    ~CasePatternVariantLiteralAst() override;
    auto to_rust() const -> std::string override;
};
