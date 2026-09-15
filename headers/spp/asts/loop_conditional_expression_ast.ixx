module;
#include <spp/macros.hpp>

export module spp.asts.loop_conditional_expression_ast;
import spp.asts.ast_kind;
import spp.asts.loop_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LoopConditionalExpressionAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LoopConditionalExpressionAst final : LoopExpressionAst {
  SPP_AST_KEY_FUNCTIONS(LoopConditionalExpressionAst);

  /// The loop condition, an expression evaluating to a boolean.
  Unique<ExpressionAst> Cond;

  LoopConditionalExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Cond) &&cond,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block);

  ~LoopConditionalExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

  /// Mark this loop as the product of desugaring an iterable
  /// loop. Such a loop runs its body once more than it yields
  /// values (the final iteration discovers the generator is
  /// exhausted), so it must not record itself as "entered" at
  /// the top of its body; the yield branch of its "case" block
  /// does that instead. Without this, the "else" block would
  /// never run, because an empty generator still enters the
  /// body once.
  auto MarkAsIterDesugar() -> void;

private:
  /// Whether this loop was desugared from an iterable loop.
  /// See "MarkAsIterDesugar".
  bool _IterDesugar;
};
