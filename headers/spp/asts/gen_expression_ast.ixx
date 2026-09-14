module;
#include <spp/macros.hpp>

export module spp.asts.gen_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenExpressionAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// Represents a value being yielded out of a coroutine. A
/// convention can be applied to the value, to create
/// foundational structures like iterators.
SPP_EXP_CLS struct spp::asts::GenExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(GenExpressionAst);

  /// The "gen" token, marking the point where the coroutine
  /// suspends its execution and yields a value.
  Unique<TokenAst> TokGen;

  /// An optional convention applied to the yielded value,
  /// such as making it an iterator.
  Unique<ConventionAst> Conv;

  /// The expression being yielded out of the coroutine; the
  /// value returned when the coroutine is resumed.
  Unique<ExpressionAst> Expr;

  GenExpressionAst(
    decltype(TokGen) &&tok_gen,
    decltype(Conv) &&conv,
    decltype(Expr) &&expr);

  ~GenExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
  Shared<TypeAst> _GenType;
  bool _IsOnce;
};
