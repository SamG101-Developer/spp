module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
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
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Expr, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Expr));

  case_utils::CreateAndAnalysePatternEqFnsDummyCore(
    {this}, sm, meta);
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

auto CasePatternVariantExpressionAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Transform the pattern into comptime values; all need to
  // be true.
  auto comptime_transforms = case_utils::CreateAndAnalysePatternEqCompTime(
    {this}, sm, meta);

  // Return the single result (only one expression will be here).
  meta->CompTimeResult = std::move(comptime_transforms[0]);
}

auto CasePatternVariantExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS;

  // Generate the LLVM.
  const auto llvm_master_transform = case_utils::CreateAndAnalysePatternEqFnsLlvm(
    {this}, sm, meta, ctx);
  return llvm_master_transform[0];
}

SPP_MOD_END
