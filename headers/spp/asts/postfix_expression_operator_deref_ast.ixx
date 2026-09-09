module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct PostfixExpressionOperatorDerefAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorDerefAst final : PostfixExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorDerefAst);

  /**
   * The @c * token that indicates a dereference operation. This is used to extract a copyable value from a borrow.
   */
  Unique<TokenAst> TokDeref;

  /**
   * Construct the PostfixExpressionOperatorDerefAst with the arguments matching the members.
   * @param tok_deref The @c * token that indicates a dereference operation.
   */
  explicit PostfixExpressionOperatorDerefAst(
    decltype(TokDeref) &&tok_deref);

  ~PostfixExpressionOperatorDerefAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::PostfixExpressionOperatorDerefAst)
