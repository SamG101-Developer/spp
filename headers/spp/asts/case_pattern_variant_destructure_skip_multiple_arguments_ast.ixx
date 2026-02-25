module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureSkipMultipleArgumentsAst;
    SPP_EXP_CLS struct CasePatternVariantSingleIdentifierAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst final : CasePatternVariantAst {
    std::unique_ptr<TokenAst> tok_ellipsis;
    std::unique_ptr<CasePatternVariantSingleIdentifierAst> binding;

    CasePatternVariantDestructureSkipMultipleArgumentsAst(
        decltype(tok_ellipsis) &&tok_ellipsis,
        std::unique_ptr<CasePatternVariantAst> &&binding);
    ~CasePatternVariantDestructureSkipMultipleArgumentsAst() override;
    auto to_rust() const -> std::string override;
};
