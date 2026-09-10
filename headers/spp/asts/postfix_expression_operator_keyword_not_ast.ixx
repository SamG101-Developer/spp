module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct PostfixExpressionOperatorKeywordNotAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordNotAst final : PostfixExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorKeywordNotAst);

  /**
   * The @c . token that indicates a member access operation.
   */
  Unique<TokenAst> TokDot;

  /**
   * The @c not token that indicates a keyword not operation.
   */
  Unique<TokenAst> TokNot;

  /**
   * Construct the PostfixExpressionOperatorKeywordNotAst with the arguments matching the members.
   * @param tok_dot The @c . token that indicates a member access operation.
   * @param tok_not The @c not token that indicates a keyword not operation.
   */
  PostfixExpressionOperatorKeywordNotAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokNot) &&tok_not);

  ~PostfixExpressionOperatorKeywordNotAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::PostfixExpressionOperatorKeywordNotAst)
