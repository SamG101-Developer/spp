#ifndef CASE_PATTERN_VARIANT_SINGLE_IDENTIFIER_AST_HPP
#define CASE_PATTERN_VARIANT_SINGLE_IDENTIFIER_AST_HPP

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantSingleIdentifierAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional @c mut token that indicates the pattern is mutable. If no @c mut token is present, the introduced
     * variable is not mutable.
     */
    std::unique_ptr<TokenAst> tok_mut;

    /**
     * The name of the single identifier pattern. This is the identifier that is used to refer to the variable being
     * introduced by the pattern.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The optional alias for the single identifier pattern. This will cause the matching to happen against @c name, but
     * introduce a variable whose name is the alias.
     */
    std::unique_ptr<LocalVariableSingleIdentifierAliasAst> alias;

    /**
     * Construct the CasePatternVariantSingleIdentifierAst with the arguments matching the members.
     * @param tok_mut The optional @c mut token that indicates the pattern is mutable.
     * @param name The name of the single identifier pattern.
     * @param alias The optional alias for the single identifier pattern.
     */
    CasePatternVariantSingleIdentifierAst(
        decltype(tok_mut) &&tok_mut,
        decltype(name) &&name,
        decltype(alias) &&alias);
};


#endif //CASE_PATTERN_VARIANT_SINGLE_IDENTIFIER_AST_HPP
