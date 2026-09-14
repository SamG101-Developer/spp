module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantElseCaseAst);
use(spp::asts, struct CaseExpressionAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantElseCaseAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantElseCaseAst);

  /// The "else" keyword marking this as an else branch of the
  /// case pattern variant.
  Unique<TokenAst> TokElse;

  /// The case expression used for the else branch.
  Unique<CaseExpressionAst> CaseExpr;

  explicit CasePatternVariantElseCaseAst(
    decltype(TokElse) &&tok_else,
    decltype(CaseExpr) &&case_expr);

  ~CasePatternVariantElseCaseAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
