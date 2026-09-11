module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_await_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorKeywordAwaitAst) {
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordAwaitAst final : PostfixExpressionOperatorAst {
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

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;

private:
  Shared<PostfixExpressionAst> _MappedFunc;
};
