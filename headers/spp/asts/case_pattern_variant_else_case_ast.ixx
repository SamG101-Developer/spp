module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct CasePatternVariantElseCaseAst;
  SPP_EXP_CLS struct CaseExpressionAst;
  SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::CasePatternVariantElseCaseAst final : CasePatternVariantAst {
  SPP_GCC_VTABLE_FIX
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

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::CasePatternVariantElseCaseAst)
