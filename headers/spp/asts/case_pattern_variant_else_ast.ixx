module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantElseAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantElseAst final : CasePatternVariantAst {
    std::unique_ptr<TokenAst> tok_else;

    explicit CasePatternVariantElseAst(
        decltype(tok_else) &&tok_else);
    ~CasePatternVariantElseAst() override;
    auto to_rust() const -> std::string override;
};
