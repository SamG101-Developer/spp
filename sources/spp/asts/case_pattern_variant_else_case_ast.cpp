module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
  decltype(TokElse) &&tok_else,
  decltype(CaseExpr) &&case_expr) :
  TokElse(std::move(tok_else)),
  CaseExpr(std::move(case_expr)) {
}

CasePatternVariantElseCaseAst::~CasePatternVariantElseCaseAst() = default;

auto CasePatternVariantElseCaseAst::PosStart() const -> std::size_t {
  // Use the "else" token.
  return TokElse->PosStart();
}

auto CasePatternVariantElseCaseAst::PosEnd() const -> std::size_t {
  // Use the [case expression]'s condition.
  return CaseExpr->Cond->PosEnd();
}

auto CasePatternVariantElseCaseAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantElseCaseAst>(
    AstClone(TokElse),
    AstClone(CaseExpr));
}

auto CasePatternVariantElseCaseAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokElse);
  SPP_STRING_APPEND(CaseExpr);
  SPP_STRING_END;
}

auto CasePatternVariantElseCaseAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward analysis into the case expression.
  CaseExpr->Stage7_AnalyseSemantics(sm, meta);
}

auto CasePatternVariantElseCaseAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward memory checks into the case expression.
  CaseExpr->Stage8_CheckMemory(sm, meta);
}

auto CasePatternVariantElseCaseAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Get the comptime result from the case expression.
  CaseExpr->Stage9_CompTimeResolve(sm, meta);
}

auto CasePatternVariantElseCaseAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Delegate code generation to the case expression.
  return CaseExpr->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
