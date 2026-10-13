module;
#include <spp/macros.hpp>

export module spp.asts.loop_iterable_expression_ast;
import spp.asts.ast_kind;
import spp.asts.loop_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LoopIterableExpressionAst);
use(spp::asts, struct DeferStatementAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct LoopConditionalExpressionAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::LoopIterableExpressionAst final : LoopExpressionAst {
  SPP_AST_KEY_FUNCTIONS(LoopIterableExpressionAst);

  /// The iteration variable, filled with each element of the
  /// iterable as the loop iterates.
  Unique<LocalVariableAst> Var;

  /// The "in" keyword, separating the variable from the
  /// iterable being iterated over.
  Unique<TokenAst> TokIn;

  /// The iterable expression to iterate over. This can be any
  /// expression evaluating to a generator type, typically the
  /// ".iter_xxx" family of methods.
  Unique<ExpressionAst> Iterable;

  LoopIterableExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Var) &&var,
    decltype(TokIn) &&tok_in,
    decltype(Iterable) &&iterable,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block);

  ~LoopIterableExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// The type of an iterable loop is the type of the boolean
  /// loop it is desugared into. The base implementation cannot
  /// be used, because the "else" block and every "exit"
  /// statement belong to the transformed loop, not this node.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

private:
  Unique<LetStatementInitializedAst> _TransformedLet;

  /// The "let mut $_ok_... = true" statement holding the
  /// transformed loop's continuation flag. Exhausting the
  /// generator clears the flag rather than exiting the loop,
  /// so the loop leaves through its condition and the "else"
  /// block runs.
  Unique<LetStatementInitializedAst> _TransformedFlagLet;

  Unique<LoopConditionalExpressionAst> _TransformedLoop;

  /// "defer std::mem::ops::drop($_iter_...)", registered in the
  /// enclosing scope only while the loop runs: every way out of
  /// the loop that also leaves that scope ("ret", "exit exit")
  /// drops the iterator.
  Unique<DeferStatementAst> _IterDefer;

  /// "std::mem::ops::drop($_iter_...)" after the loop, on every
  /// other way out of it (its condition, its "else", an "exit" of
  /// this loop). It ends the iterator's loans there.
  Unique<ExpressionAst> _IterDrop;

  /// The name of the desugared iterator variable ("$_iter_...").
  Shared<IdentifierAst> _IterableName;
};
