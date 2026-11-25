module;
#include <spp/macros.hpp>

export module spp.asts.loop_conditional_expression_ast;
import spp.analyse.utils.drop_utils;
import spp.asts.ast_kind;
import spp.asts.loop_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use_common(LoopConditionalExpressionAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LoopConditionalExpressionAst final : LoopExpressionAst {
  SPP_AST_KEY_FUNCTIONS(LoopConditionalExpressionAst);

  /// The loop condition, an expression evaluating to a boolean.
  Unique<ExpressionAst> Cond;

  /// A value of the scope around the loop that ends with it: each
  /// way out of the loop (an "exit", the condition ending it) drops
  /// it, and after the loop it is gone. Never written: an iterable
  /// loop's desugaring names its iterator ("$_iter"), so the borrow
  /// it holds ends with the loop. A "ret" or an "exit" past this
  /// loop drops it with the scope it is declared in.
  Shared<IdentifierAst> EndsWithLoop;

  /// The iterable loop this one was lowered from, which an error
  /// about the loop as a whole points at (this one has no "loop"
  /// written); nullptr for a loop written as one.
  Ast const *LoweredFrom = nullptr;

  LoopConditionalExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Cond) &&cond,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block);

  ~LoopConditionalExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  // Todo: Break up and massively tidy
  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

private:
  // Its drops ("drop_utils::DropsOn"): where the condition ends the
  // loop ("kCondEnd", through the "else" block, if any), what that
  // path holds of values around the loop that another path out (an
  // "exit") does not, and what the condition's temporaries hold; at
  // the body's end, what the condition's temporaries still own
  // ("kIteration"); and at the loop's head, where its entry and its
  // ways back round (the body's end, each "skip") meet, what one holds
  // of values around the loop that another does not, dropped on each
  // still holding it ("kEntry", "kBackEdge"; a "skip"'s go on it).
};
