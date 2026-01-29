module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.case_pattern_variant_ast;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantSingleIdentifierAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAliasAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantSingleIdentifierAst final : CasePatternVariantAst {
    /**
     * The optional convention attached to the single identifier pattern. This indicates how the variable being
     * introduced by the pattern should be treated, such as by reference or by mutable reference. Mutually exclusive
     * with the @c mut token (but both can be absent).
     */
    std::unique_ptr<ConventionAst> conv;

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
     * @param conv The optional convention attached to the single identifier pattern.
     * @param tok_mut The optional @c mut token that indicates the pattern is mutable.
     * @param name The name of the single identifier pattern.
     * @param alias The optional alias for the single identifier pattern.
     */
    CasePatternVariantSingleIdentifierAst(
        decltype(conv) &&conv,
        decltype(tok_mut) &&tok_mut,
        decltype(name) &&name,
        decltype(alias) &&alias);

    ~CasePatternVariantSingleIdentifierAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
