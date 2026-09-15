module;
#include <spp/macros.hpp>

export module spp.asts.defer_statement_ast;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(DeferStatementAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// Holds an expression to run when the scope it is written in is
/// left, by whichever path leaves it: falling off the end, a
/// "ret" - including the one the "?" operator generates - or a
/// loop "exit" or "skip".
SPP_EXP_CLS struct spp::asts::DeferStatementAst final : StatementAst {
  SPP_AST_KEY_FUNCTIONS(DeferStatementAst);

  /// The "defer" token that starts this statement.
  Unique<TokenAst> TokDefer;

  /// The expression to run when the enclosing scope is left. It
  /// produces no value - nothing is in a position to receive
  /// one - so it must be "Void" typed.
  Unique<ExpressionAst> Expr;

  DeferStatementAst(
    decltype(TokDefer) &&tok_defer,
    decltype(Expr) &&expr);

  ~DeferStatementAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  Vec<Shared<IdentifierAst>> Consumed;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
