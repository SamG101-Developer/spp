module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.utils.case_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;

SPP_MOD_BEGIN
CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
  decltype(Literal) &&literal) :
  Literal(std::move(literal)) {
}

CasePatternVariantLiteralAst::~CasePatternVariantLiteralAst() = default;

auto CasePatternVariantLiteralAst::PosStart() const -> std::size_t {
  // Use the literal.
  return Literal->PosStart();
}

auto CasePatternVariantLiteralAst::PosEnd() const -> std::size_t {
  // Use the literal.
  return Literal->PosEnd();
}

auto CasePatternVariantLiteralAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantLiteralAst>(
    AstClone(Literal));
}

auto CasePatternVariantLiteralAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Literal);
  SPP_STRING_END;
}

auto CasePatternVariantLiteralAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward analysis into the literal.
  Literal->Stage7_AnalyseSemantics(sm, meta);
}

auto CasePatternVariantLiteralAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward memory checks into the literal.
  Literal->Stage8_CheckMemory(sm, meta);
}

auto CasePatternVariantLiteralAst::Stage9_CompTimeResolve(
  ScopeManager *, CompilerMetaData *meta) -> void {
  // The "case"'s plan tests the literal ("PatternSolver"), so the
  // pattern itself always matches.
  meta->CompTimeResult = BooleanLiteralAst::True(PosStart());
}

auto CasePatternVariantLiteralAst::Stage11_CodeGen(
  ScopeManager *, CompilerMetaData *, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // The "case"'s plan tests the literal ("PatternSolver"), so the
  // pattern itself always matches.
  return llvm::ConstantInt::getTrue(*ctx->Context);
}

auto CasePatternVariantLiteralAst::ConvToVar(
  CompilerMetaData *) -> Unique<LocalVariableAst> {
  IMPORT_UTILS_AND_UID;
  // Create the local variable literal binding AST.
  const auto uid = Uid();
  auto var_name = MakeShared<IdentifierAst>(PosStart(), uid);
  auto var = MakeUnique<LocalVariableSingleIdentifierAst>(
    nullptr, std::move(var_name), nullptr);
  var->MarkFromCasePattern();
  return var;
}

SPP_MOD_END
