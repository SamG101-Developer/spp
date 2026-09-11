module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_async_ast;
import spp.asts.ast_kind;
import spp.asts.unary_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(UnaryExpressionOperatorAsyncAst) {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAsyncAst final : UnaryExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(UnaryExpressionOperatorAsyncAst);

  /**
   * The @c async keyword that indicates an asynchronous operation. This is used to mark the following function call
   * as called asynchronously.
   */
  Unique<TokenAst> TokAsync;

  struct {
    Unique<ExpressionAst> _OriginalRhs;
  } Source;

  /**
   * Construct the UnaryExpressionOperatorAsyncAst with the arguments matching the members.
   * @param tok_async The @c async keyword that indicates an asynchronous operation.
   */
  explicit UnaryExpressionOperatorAsyncAst(
    decltype(TokAsync) &&tok_async);

  ~UnaryExpressionOperatorAsyncAst() override;

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

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;

private:
  Unique<ExpressionAst> _TransformedFunc;
};
