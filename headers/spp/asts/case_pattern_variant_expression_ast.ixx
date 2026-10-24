module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_expression_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantExpressionAst);
use(spp::asts, struct ExpressionAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantExpressionAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantExpressionAst);

  /// The expression matched against the condition from the
  /// "case" statement.
  Unique<ExpressionAst> Expr;

  explicit CasePatternVariantExpressionAst(
    decltype(Expr) &&expr);

  ~CasePatternVariantExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
