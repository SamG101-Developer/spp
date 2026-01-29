module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantElseCaseAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantElseCaseAst final : CasePatternVariantAst {
    /**
     * The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * The case expression that is used for the else branch.
     */
    std::unique_ptr<CaseExpressionAst> case_expr;

    /**
     * Construct the CasePatternVariantElseCaseAst with the arguments matching the members.
     * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
     * @param case_expr The case expression that is used for the else branch.
     */
    explicit CasePatternVariantElseCaseAst(
        decltype(tok_else) &&tok_else,
        decltype(case_expr) &&case_expr);

    ~CasePatternVariantElseCaseAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
