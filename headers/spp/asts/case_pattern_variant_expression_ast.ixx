module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantExpressionAst;
    SPP_EXP_CLS struct ExpressionAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
    /**
     * The expression that is used in the case pattern variant. This is the expression that will be matched against the
     * condition from the @c case statement.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Construct the CasePatternVariantExpressionAst with the arguments matching the members.
     * @param expr The expression that is used in the case pattern variant.
     */
    explicit CasePatternVariantExpressionAst(
        decltype(Expr) &&expr);

    ~CasePatternVariantExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
