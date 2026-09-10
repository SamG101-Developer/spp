module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_await_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct PostfixExpressionOperatorKeywordAwaitAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordAwaitAst final : PostfixExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorKeywordAwaitAst);

  /**
   * The @c . token that indicates a member access operation.
   */
  Unique<TokenAst> TokDot;

  /**
   * The @c await token that indicates a keyword await operation.
   */
  Unique<TokenAst> TokAwait;

  /**
   * Construct the PostfixExpressionOperatorKeywordAwaitAst with the arguments matching the members.
   * @param tok_dot The @c . token that indicates a member access operation.
   * @param tok_await The @c await token that indicates a keyword await operation.
   */
  PostfixExpressionOperatorKeywordAwaitAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokAwait) &&tok_await);

  ~PostfixExpressionOperatorKeywordAwaitAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

private:
  Shared<PostfixExpressionAst> _MappedFunc;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::PostfixExpressionOperatorKeywordAwaitAst)
