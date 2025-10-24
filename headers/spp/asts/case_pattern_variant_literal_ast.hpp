#pragma once
#include <spp/asts/case_pattern_variant_ast.hpp>


struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
    /**
     * The literal value of the case pattern variant. This can be a string, integer, float, boolean, but not a tuple or
     * array; special destructure syntax exists for those literals.
     */
    std::unique_ptr<LiteralAst> literal;

    /**
     * Construct the CasePatternVariantLiteralAst with the arguments matching the members.
     * @param literal The literal value of the case pattern variant.
     */
    explicit CasePatternVariantLiteralAst(
        decltype(literal) &&literal);

    ~CasePatternVariantLiteralAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert_to_variable(mixins::CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, mixins::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value * override;
};
