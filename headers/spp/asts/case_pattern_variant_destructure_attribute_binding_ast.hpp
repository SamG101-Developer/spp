#ifndef CASE_PATTERN_VARIANT_DESTRUCTURE_ATTRIBUTE_BINDING_AST_HPP
#define CASE_PATTERN_VARIANT_DESTRUCTURE_ATTRIBUTE_BINDING_AST_HPP

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantDestructureAttributeBindingAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name of the attribute. This is the identifier that is used to refer to the attribute of the object being
     * destructured.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The @c = token that separates the attribute name from the value in the destructure binding.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The value of the attribute. This can be a further destructure or a literal.
     */
    std::unique_ptr<CasePatternVariantAst> val;

    /**
     * Construct the CasePatternVariantDestructureAttributeBindingAst with the arguments matching the members.
     * @param name The name of the attribute.
     * @param tok_assign The @c = token that separates the attribute name from the value in the destructure binding.
     * @param val The value of the attribute.
     */
    CasePatternVariantDestructureAttributeBindingAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);
};


#endif //CASE_PATTERN_VARIANT_DESTRUCTURE_ATTRIBUTE_BINDING_AST_HPP
