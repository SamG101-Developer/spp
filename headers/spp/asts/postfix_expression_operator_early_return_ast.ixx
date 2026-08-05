module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct InnerScopeExpressionAst;
  SPP_EXP_CLS struct PostfixExpressionOperatorEarlyReturnAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
  /**
   * The @c ? token that indicates an early return in a postfix expression. This token is used to signify that the
   * expression should be checked for its result-type failure type, and if it matches, the expression will lift the
   * error to the caller.
   */
  Unique<TokenAst> TokQst;

  /**
   * Construct the PostfixExpressionOperatorEarlyReturnAst with the arguments matching the members.
   * @param[in] tok_qst The @c ? token that indicates an early return in a postfix expression.
   */
  explicit PostfixExpressionOperatorEarlyReturnAst(
    decltype(TokQst) &&tok_qst);

  ~PostfixExpressionOperatorEarlyReturnAst() override;

  SPP_AST_KEY_FUNCTIONS;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
  /**
   * The lowered form of the "?" operator, built and analysed in @c Stage7_AnalyseSemantics and forwarded to by every
   * later stage:
   * @code
   * {
   *     let $temp = <lhs>
   *     case $temp is <Residual>(..) { ret $temp }   # "gen $temp" then a bare "ret" in a coroutine
   *     $temp.op_as_value()
   * }
   * @endcode
   * The left-hand-side is bound to a temporary because it is referred to three times (the residual test, the early
   * return, and the value extraction), so evaluating it in place would run any side effect three times - and the
   * residual test needs a plain identifier anyway, since that is what lets the case pattern flow-type the condition
   * and read its discriminant.
   */
  Shared<InnerScopeExpressionAst> _TransformedExpr;
};
