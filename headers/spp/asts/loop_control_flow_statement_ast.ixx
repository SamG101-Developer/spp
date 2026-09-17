module;
#include <spp/macros.hpp>

export module spp.asts.loop_control_flow_statement_ast;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LoopControlFlowStatementAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::LoopControlFlowStatementAst final : StatementAst {
  SPP_AST_KEY_FUNCTIONS(LoopControlFlowStatementAst);

  /// The "exit" tokens, allowing a statement to exit an
  /// arbitrary number of loops. If there are none, the "skip"
  /// token will be present; the parser ensures this.
  Vec<Unique<TokenAst>> TokSeqExit;

  /// The optional "skip" token, to skip the loop iteration.
  /// Usable with or without "exit" tokens; "exit exit skip"
  /// exits the innermost 2 loops, then skips the iteration of
  /// the 3rd. A "skip" and a value are mutually exclusive, and
  /// the parser prevents both from being present.
  Unique<TokenAst> TokSkip;

  /// The expression returned to the loop's assignment
  /// variable. For "let x = loop { ... }", "x" can be assigned
  /// a value with the "exit value" statement.
  Unique<ExpressionAst> Expr;

  LoopControlFlowStatementAst(
    decltype(TokSeqExit) &&tok_seq_exit,
    decltype(TokSkip) &&tok_skip,
    decltype(Expr) &&expr);

  ~LoopControlFlowStatementAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Whether control leaves this scope here, which an "exit"
  /// or a "skip" always does. Without this, the branch it sits
  /// in reads as falling through, so the memory state it left
  /// behind - values it moved before jumping - is applied to
  /// the code after the "case", which then sees them as moved
  /// on a path that never ran.
  SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;
};
