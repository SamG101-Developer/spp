module;
#include <spp/macros.hpp>

module spp.asts.spread_expression_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.ice;

SPP_MOD_BEGIN
SpreadExpressionAst::SpreadExpressionAst(
  decltype(TokUnpack) &&tok_unpack,
  decltype(Expr) &&expr) :
  TokUnpack(std::move(tok_unpack)),
  Expr(std::move(expr)) {
}

SpreadExpressionAst::~SpreadExpressionAst() = default;

auto SpreadExpressionAst::PosStart() const -> std::size_t {
  return TokUnpack->PosStart();
}

auto SpreadExpressionAst::PosEnd() const -> std::size_t {
  return Expr->PosEnd();
}

auto SpreadExpressionAst::Clone() const -> Unique<Ast> {
  return MakeUnique<SpreadExpressionAst>(
    AstClone(TokUnpack),
    AstClone(Expr));
}

auto SpreadExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokUnpack);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto SpreadExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // A spread may name a pack whole ("(..xs)"); nothing under it may.
  const auto _pack_guard = meta::AllowPackWholeGuard(
    meta, Expr->To<IdentifierAst>() != nullptr);
  Expr->Stage7_AnalyseSemantics(sm, meta);
}

auto SpreadExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  Expr->Stage8_CheckMemory(sm, meta);
}

auto SpreadExpressionAst::Stage11_CodeGen(
  ScopeManager *, CompilerMetaData *, codegen::LlvmCtx *) -> llvm::Value* {
  // Only a template's open pack keeps its spread, and no template is generated.
  Ice(*this, "a spread reached code generation unexpanded");
}

auto SpreadExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The tuple's type: the literal holding it takes its elements.
  return Expr->InferType(sm, meta);
}

auto SpreadExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return Expr->InferTypeRef(sm, meta);
}

auto SpreadExpressionAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  return MakeShared<SpreadExpressionAst>(
    AstClone(TokUnpack), AstClone(Expr->ReadExpr(sub)));
}

SPP_MOD_END
