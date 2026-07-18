module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureAttributeBindingAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureAttributeBindingAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name of the attribute. This is the identifier that is used to refer to the attribute of the object being
     * destructured.
     */
    Shared<IdentifierAst> Name;

    /**
     * The @c = token that separates the attribute name from the value in the destructure binding.
     */
    Unique<TokenAst> TokAssign;

    /**
     * The value of the attribute. This can be a further destructure or a literal.
     */
    Unique<CasePatternVariantAst> Val;

    /**
     * Construct the CasePatternVariantDestructureAttributeBindingAst with the arguments matching the members.
     * @param name The name of the attribute.
     * @param tok_assign The @c = token that separates the attribute name from the value in the destructure binding.
     * @param val The value of the attribute.
     */
    CasePatternVariantDestructureAttributeBindingAst(
        decltype(Name) &&name,
        decltype(TokAssign) &&tok_assign,
        decltype(Val) &&val);

    ~CasePatternVariantDestructureAttributeBindingAst() override;

    auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
