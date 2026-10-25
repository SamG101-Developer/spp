module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.unary_expression_operator_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
UnaryExpressionAst::UnaryExpressionAst(
  decltype(Op) &&tok_op,
  decltype(Expr) &&expr) :
  Op(std::move(tok_op)),
  Expr(std::move(expr)) {
}

UnaryExpressionAst::~UnaryExpressionAst() = default;

auto UnaryExpressionAst::PosStart() const -> std::size_t {
  // Use the operator.
  return Op->PosStart();
}

auto UnaryExpressionAst::PosEnd() const -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto UnaryExpressionAst::Clone() const -> Unique<Ast> {
  return MakeUnique<UnaryExpressionAst>(
    AstClone(Op),
    AstClone(Expr));
}

auto UnaryExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Op);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto UnaryExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // An operator that rebuilds its operand ("async" into a closure)
  // is handed a copy of it as written, and owns every analysis of
  // it.
  if (Op->RebuildsOperand()) {
    const auto _meta_guard = MetaGuard(meta);
    Op->WrittenOperand = AstClone(Expr);
    Op->Stage7_AnalyseSemantics(sm, meta);
    return;
  }

  // Analyse the operator and right-hand-side expression.
  Expr->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Expr, *sm),
    ERR_ARGS(*Expr));

  const auto _meta_guard = MetaGuard(meta);
  Op->Stage7_AnalyseSemantics(sm, meta);
}

auto UnaryExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the memory of the right-hand-side, unless the operator
  // walks it itself, and update the meta context to track the rhs
  // part of this unary expression.
  if (not Op->WalksOperand()) { Expr->Stage8_CheckMemory(sm, meta); }
  const auto _meta_guard = MetaGuard(meta);
  Op->Stage8_CheckMemory(sm, meta);
}

auto UnaryExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate the right-hand-side expression.
  const auto _meta_guard = MetaGuard(meta);
  const auto lhs_val = Op->Stage11_CodeGen(sm, meta, ctx);
  return lhs_val;
}

auto UnaryExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Infer the type of the right-hand-side expression,
  // adjusted by the operator.
  const auto _meta_guard = MetaGuard(meta);
  auto type = Op->InferType(sm, meta);

  return type;
}

auto UnaryExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  const auto _meta_guard = MetaGuard(meta);
  return Op->InferTypeRef(sm, meta);
}

auto UnaryExpressionAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // The only unary operator is the "async" function call
  // so there will be no specialization.
  return MakeShared<UnaryExpressionAst>(AstClone(Op), AstClone(Expr->ReadExpr(sub)));
}

auto UnaryExpressionAst::IsAllowedInDefault() const -> bool {
  // Check the op and the rhs expression. Nullptr guard in
  // case of std::move(ast) being used (not 100% sure if
  // needed or not).
  return
    (Op == nullptr or Op->IsAllowedInDefault()) and
    (Expr == nullptr or Expr->IsAllowedInDefault());
}

SPP_MOD_END
