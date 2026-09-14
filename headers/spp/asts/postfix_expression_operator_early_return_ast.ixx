module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorEarlyReturnAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorEarlyReturnAst);

  /// The "?" token that indicates an early return. The
  /// expression is checked for its result-type failure type,
  /// and if it matches, the error is lifted to the caller.
  Unique<TokenAst> TokQst;

  explicit PostfixExpressionOperatorEarlyReturnAst(
    decltype(TokQst) &&tok_qst);

  ~PostfixExpressionOperatorEarlyReturnAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
  /// The lowered form of the "?" operator, built and analysed
  /// in "Stage7_AnalyseSemantics" and forwarded to by every
  /// later stage:
  ///
  ///   {
  ///       let $temp = <lhs>
  ///       case $temp is <Residual>(..) { ret $temp }
  ///       $temp.op_as_value()
  ///   }
  ///
  /// In a coroutine, the "ret $temp" is "gen $temp" then a bare
  /// "ret". The left-hand-side is bound to a temporary because
  /// it is referred to three times (the residual test, the
  /// early return, and the value extraction), so evaluating it
  /// in place would run any side effect three times - and the
  /// residual test needs a plain identifier anyway, since that
  /// lets the case pattern flow-type the condition and read its
  /// discriminant.
  Shared<InnerScopeExpressionAst> _TransformedExpr;
};
