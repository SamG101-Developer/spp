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
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct DeferStatementAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LoopConditionalExpressionAst final : LoopExpressionAst {
  SPP_AST_KEY_FUNCTIONS(LoopConditionalExpressionAst);

  /// The loop condition, an expression evaluating to a boolean.
  Unique<ExpressionAst> Cond;

  /// A "defer" in the loop's own scope: it runs as the loop is
  /// left - by an "exit", by the condition ending it (after the
  /// "else"), or by a "ret" from inside it - but not between
  /// iterations. Never written: an iterable loop's desugaring
  /// gives one, to drop its iterator.
  Unique<DeferStatementAst> LoopDefer;

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

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

};
