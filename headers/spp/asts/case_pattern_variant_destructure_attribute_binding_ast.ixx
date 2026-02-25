module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureAttributeBindingAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureAttributeBindingAst final : CasePatternVariantAst {
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<TokenAst> tok_assign;
    std::unique_ptr<CasePatternVariantAst> val;

    CasePatternVariantDestructureAttributeBindingAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);
    ~CasePatternVariantDestructureAttributeBindingAst() override;
    auto to_rust() const -> std::string override;
};
