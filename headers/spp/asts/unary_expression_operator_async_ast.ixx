module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_async_ast;
import spp.asts.ast_kind;
import spp.asts.unary_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(UnaryExpressionOperatorAsyncAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAsyncAst final : UnaryExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(UnaryExpressionOperatorAsyncAst);

  /// The "async" keyword, marking the following function call
  /// as called asynchronously.
  Unique<TokenAst> TokAsync;

  struct {
    Unique<ExpressionAst> _OriginalRhs;
  } Source;

  explicit UnaryExpressionOperatorAsyncAst(
    decltype(TokAsync) &&tok_async);

  ~UnaryExpressionOperatorAsyncAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  Unique<ExpressionAst> _TransformedFunc;
};
