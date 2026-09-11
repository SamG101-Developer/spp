module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_expression_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantExpressionAst) {
  SPP_EXP_CLS struct ExpressionAst;
}

SPP_EXP_CLS struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantExpressionAst);

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

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;
};
