#pragma once
#include <spp/asts/case_pattern_variant_ast.hpp>


struct spp::asts::CasePatternVariantElseAst final : CasePatternVariantAst {
    /**
     * The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * Construct the CasePatternVariantElseAst with the arguments matching the members.
     * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    explicit CasePatternVariantElseAst(
        decltype(tok_else) &&tok_else);

    ~CasePatternVariantElseAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_10_code_gen_2(ScopeManager *sm, mixins::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value * override;
};
