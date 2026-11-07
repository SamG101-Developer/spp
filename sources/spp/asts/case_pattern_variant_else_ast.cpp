module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::CasePatternVariantElseAst::CasePatternVariantElseAst(
  decltype(TokElse) &&tok_else) :
  TokElse(std::move(tok_else)),
  _ForIterLoopExit(false) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokElse, lex::SppTokenType::KW_ELSE, "else");
}

spp::asts::CasePatternVariantElseAst::~CasePatternVariantElseAst() = default;

auto spp::asts::CasePatternVariantElseAst::PosStart() const -> std::size_t {
  // Use the "else" token.
  return TokElse->PosStart();
}

auto spp::asts::CasePatternVariantElseAst::PosEnd() const -> std::size_t {
  // Use the "else" token,
  return TokElse->PosEnd();
}

auto spp::asts::CasePatternVariantElseAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, carrying over the marker
  // the loop's desugaring set.
  auto ast = MakeUnique<CasePatternVariantElseAst>(
    AstClone(TokElse));
  ast->_ForIterLoopExit = _ForIterLoopExit;
  return ast;
}

auto spp::asts::CasePatternVariantElseAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokElse);
  SPP_STRING_END;
}

auto spp::asts::CasePatternVariantElseAst::Stage11_CodeGen(
  ScopeManager *, CompilerMetaData *, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // The "else" pattern always matches, so return "true".
  // However, should a previous branch match before this one
  // is reached, then that one will be selected.
  return llvm::ConstantInt::getTrue(*ctx->Context);
}

auto spp::asts::CasePatternVariantElseAst::EvaluateCompTime(
  ScopeManager *, CompilerMetaData *meta) -> void {
  // The "else" pattern always matches, so return "true".
  meta->CompTimeResult = BooleanLiteralAst::True(TokElse->PosStart());
}

auto spp::asts::CasePatternVariantElseAst::MarkForIterLoopExit() -> void {
  // Simple setter for marking the else from a loop exit.
  _ForIterLoopExit = true;
}

auto spp::asts::CasePatternVariantElseAst::MarkedForIterLoopExit() const -> bool {
  // Simple getter for the loop exit marker.
  return _ForIterLoopExit;
}

SPP_MOD_END
