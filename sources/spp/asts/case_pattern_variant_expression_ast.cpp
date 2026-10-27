module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
CasePatternVariantExpressionAst::CasePatternVariantExpressionAst(
  decltype(Expr) &&expr) :
  Expr(std::move(expr)) {
}

CasePatternVariantExpressionAst::~CasePatternVariantExpressionAst() = default;

auto CasePatternVariantExpressionAst::PosStart() const -> std::size_t {
  // Use the expression.
  return Expr->PosStart();
}

auto CasePatternVariantExpressionAst::PosEnd() const -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto CasePatternVariantExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantExpressionAst>(
    AstClone(Expr));
}

auto CasePatternVariantExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto CasePatternVariantExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Forward analysis into the expression.
  Expr->Stage7_AnalyseSemantics(sm, meta);
  common_errors::RequirePrimary(*Expr, *sm);
}

auto CasePatternVariantExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Check the memory of the expression. Todo: maybe
  // do this via generated == function?
  Expr->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(
    *Expr, *Expr, *sm, meta);
}

SPP_MOD_END
