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
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAsyncAst final : UnaryExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(UnaryExpressionOperatorAsyncAst);

  /// The "async" keyword, marking the following function call
  /// as called asynchronously.
  Unique<TokenAst> TokAsync;

  explicit UnaryExpressionOperatorAsyncAst(
    decltype(TokAsync) &&tok_async);

  ~UnaryExpressionOperatorAsyncAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  SPP_ATTR_NODISCARD auto RebuildsOperand() const -> bool override;

private:
  Unique<ExpressionAst> _TransformedFn;

  /// A copy of the operand as written, analysed only to type the
  /// future: the closure is built from the operand as written, and
  /// that is what the later stages walk.
  Unique<ExpressionAst> _TypedOperand;

  /// The future type, "Fut[T]", built and analysed once from
  /// "_TypedOperand", and what "InferType" answers.
  Shared<TypeAst> _FutType;

  /// The scope "_TypedOperand" is analysed in. Its scopes ("?", a
  /// block in an argument) are kept in this one, which the later
  /// stages skip as one, so they are not mistaken for the closure's.
  Scope *_OperandScope = nullptr;

  /// Step the walk past "_OperandScope", before the closure's
  /// scopes (it was created before them).
  auto _SkipOperandScope(ScopeManager *sm) const -> void;
};
