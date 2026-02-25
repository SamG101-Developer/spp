module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantSingleIdentifierAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAliasAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantSingleIdentifierAst final : CasePatternVariantAst {
    std::unique_ptr<ConventionAst> conv;
    std::unique_ptr<TokenAst> tok_mut;
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<LocalVariableSingleIdentifierAliasAst> alias;

    explicit CasePatternVariantSingleIdentifierAst(
        decltype(conv) &&conv,
        decltype(tok_mut) &&tok_mut,
        decltype(name) &&name,
        decltype(alias) &&alias);
    ~CasePatternVariantSingleIdentifierAst() override;
    auto to_rust() const -> std::string override;
};
