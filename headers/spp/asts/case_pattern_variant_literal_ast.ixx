module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_literal_ast;
import spp.asts._fwd;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantLiteralAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
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

    auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
