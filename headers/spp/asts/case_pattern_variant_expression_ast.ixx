module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_expression_ast;
import spp.codegen.llvm_ctx;
import spp.asts.case_pattern_variant_ast;

import llvm;
import std;


SPP_EXP_CLS struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
    /**
     * The expression that is used in the case pattern variant. This is the expression that will be matched against the
     * condition from the @c case statement.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the CasePatternVariantExpressionAst with the arguments matching the members.
     * @param expr The expression that is used in the case pattern variant.
     */
    explicit CasePatternVariantExpressionAst(
        decltype(expr) &&expr);

    ~CasePatternVariantExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
