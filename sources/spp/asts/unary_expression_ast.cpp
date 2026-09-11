module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.unary_expression_operator_ast;
import spp.asts.unary_expression_operator_async_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::UnaryExpressionAst::UnaryExpressionAst(
  decltype(Op) &&tok_op,
  decltype(Expr) &&expr) :
  Op(std::move(tok_op)),
  Expr(std::move(expr)) {
}

spp::asts::UnaryExpressionAst::~UnaryExpressionAst() = default;

auto spp::asts::UnaryExpressionAst::PosStart() const
  -> std::size_t {
  // Use the operator.
  return Op->PosStart();
}

auto spp::asts::UnaryExpressionAst::PosEnd() const
  -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto spp::asts::UnaryExpressionAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<UnaryExpressionAst>(
    AstClone(Op),
    AstClone(Expr));
}

auto spp::asts::UnaryExpressionAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Op);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto spp::asts::UnaryExpressionAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

  // "async" lowers the call into a closure, and has to rebuild
  // it from how it was written. Analysis rewrites a call's args
  // in place, so the copy it needs can only be taken here,
  // before that happens.
  if (const auto async_op = Op->To<UnaryExpressionOperatorAsyncAst>(); async_op != nullptr) {
    async_op->Source._OriginalRhs = AstClone(Expr);
  }

  // Analyse the operator and right-hand-side expression.
  Expr->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(*Expr, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Expr));

  const auto _meta_guard = meta::MetaGuard(meta);
  meta->UnaryExpressionRhs = Expr.get();
  Op->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::UnaryExpressionAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Check the memory of the right-hand-side, and update the
  // meta context to track the rhs part of this unary expression.
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->UnaryExpressionRhs = Expr.get();
  Op->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::UnaryExpressionAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Generate the right-hand-side expression.
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->UnaryExpressionRhs = Expr.get();
  const auto lhs_val = Op->Stage11_CodeGen(sm, meta, ctx);
  return lhs_val;
}

auto spp::asts::UnaryExpressionAst::InferType(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Infer the type of the right-hand-side expression,
  // adjusted by the operator.
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->UnaryExpressionRhs = Expr.get();
  auto type = Op->InferType(sm, meta);

  return type;
}

auto spp::asts::UnaryExpressionAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const
  -> Shared<ExpressionAst> {
  // The only unary operator is the "async" function call
  // so there will be no specialization.
  return MakeShared<UnaryExpressionAst>(AstClone(Op), AstClone(Expr->SubstituteGenericsExpr(args)));
}

auto spp::asts::UnaryExpressionAst::IsAllowedInDefault() const
  -> bool {
  // Check the op and the rhs expression. Nullptr guard in
  // case of std::move(ast) being used (not 100% sure if
  // needed or not).
  return
    (Op == nullptr or Op->IsAllowedInDefault()) and
    (Expr == nullptr or Expr->IsAllowedInDefault());
}

SPP_MOD_END
