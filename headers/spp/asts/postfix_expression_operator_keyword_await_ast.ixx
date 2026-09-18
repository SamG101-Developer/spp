module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_await_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorKeywordAwaitAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordAwaitAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorKeywordAwaitAst);

  /// The "." token that indicates a member access operation.
  Unique<TokenAst> TokDot;

  /// The "await" token that indicates a keyword await operation.
  Unique<TokenAst> TokAwait;

  PostfixExpressionOperatorKeywordAwaitAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokAwait) &&tok_await);

  ~PostfixExpressionOperatorKeywordAwaitAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  Shared<PostfixExpressionAst> _MappedFunc;
};
