module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorKeywordNotAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordNotAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorKeywordNotAst);

  /// The "." token that indicates a member access operation.
  Unique<TokenAst> TokDot;

  /// The "not" token that indicates a keyword not operation.
  Unique<TokenAst> TokNot;

  PostfixExpressionOperatorKeywordNotAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokNot) &&tok_not);

  ~PostfixExpressionOperatorKeywordNotAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
