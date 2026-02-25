module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_single_argument_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureSkipSingleArgumentAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst final : CasePatternVariantAst {
    std::unique_ptr<TokenAst> tok_underscore;

    explicit CasePatternVariantDestructureSkipSingleArgumentAst(
        decltype(tok_underscore) &&tok_underscore);
    ~CasePatternVariantDestructureSkipSingleArgumentAst() override;
    auto to_rust() const -> std::string override;
};
