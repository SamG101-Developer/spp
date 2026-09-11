module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantElseCaseAst) {
  SPP_EXP_CLS struct CaseExpressionAst;
  SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::CasePatternVariantElseCaseAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantElseCaseAst);

  /**
   * The @c else keyword that indicates this is an else branch of the case pattern variant.
   */
  Unique<TokenAst> TokElse;

  /**
   * The case expression that is used for the else branch.
   */
  Unique<CaseExpressionAst> CaseExpr;

  /**
   * Construct the CasePatternVariantElseCaseAst with the arguments matching the members.
   * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
   * @param case_expr The case expression that is used for the else branch.
   */
  explicit CasePatternVariantElseCaseAst(
    decltype(TokElse) &&tok_else,
    decltype(CaseExpr) &&case_expr);

  ~CasePatternVariantElseCaseAst() override;

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
