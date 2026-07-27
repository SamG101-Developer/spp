module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorKeywordNotAst::PostfixExpressionOperatorKeywordNotAst(
  decltype(TokDot) &&tok_dot,
  decltype(TokNot) &&tok_not) :
  TokDot(std::move(tok_dot)),
  TokNot(std::move(tok_not)) {
}

spp::asts::PostfixExpressionOperatorKeywordNotAst::~PostfixExpressionOperatorKeywordNotAst() = default;

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::PosStart() const
  -> std::size_t {
  // Use the "." token.
  return TokDot->PosStart();
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::PosEnd() const
  -> std::size_t {
  // Use the "not" token.
  return TokNot->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<PostfixExpressionOperatorKeywordNotAst>(
    AstClone(TokDot),
    AstClone(TokNot));
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokDot);
  SPP_STRING_APPEND(TokNot);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppExpressionNotBooleanError;
  using analyse::utils::type_utils::IsTypeBool;

  // Check the left-hand-side is a boolean expression.
  // Todo: Test with convention.
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  RaiseIf<SppExpressionNotBooleanError>(
    not IsTypeBool(*lhs_type->WithoutConvention(), *sm->CurrentScope),
    {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, "not expression"));
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // The "lhs" will be boolean based on previous analysis.
  meta->PostfixExpressionLhs->Stage9_CompTimeResolve(sm, meta);
  const auto cmp_lhs_bool = meta->CmpResult->To<BooleanLiteralAst>();

  // Extract the value inside the boolean and invert it.
  const auto p = PosStart();
  meta->CmpResult = cmp_lhs_bool->IsTrue() ? BooleanLiteralAst::False(p) : BooleanLiteralAst::True(p);
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // Generate the left-hand-side expression, which analysis has guaranteed is a boolean, owned or borrowed.
  const auto uid = "." + spp::utils::Uid(this);
  auto lhs_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
  SPP_ASSERT(lhs_val != nullptr);

  // A borrowed boolean is a pointer, so read the "i1" out of it first.
  if (lhs_val->getType()->isPointerTy()) {
    lhs_val = ctx->Builder.CreateLoad(llvm::Type::getInt1Ty(*ctx->Context), lhs_val, "not.load" + uid);
  }
  SPP_ASSERT(lhs_val->getType()->isIntegerTy(1));

  // Use a "not" instruction to invert the expression on the lhs.
  return ctx->Builder.CreateNot(lhs_val, "not" + uid);
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::InferType(
  ScopeManager *,
  CompilerMetaData *)
  -> Shared<TypeAst> {
  // The type of a "not" expression is always boolean.
  using generate::common_types::BooleanType;
  return BooleanType(PosStart());
}

SPP_MOD_END
