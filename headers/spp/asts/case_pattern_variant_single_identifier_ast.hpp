#pragma once
#include <spp/asts/case_pattern_variant_ast.hpp>


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
    std::shared_ptr<IdentifierAst> name;

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

    ~CasePatternVariantSingleIdentifierAst() override;

    auto convert_to_variable(mixins::CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
