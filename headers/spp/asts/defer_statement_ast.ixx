module;
#include <spp/macros.hpp>

export module spp.asts.defer_statement_ast;
import spp.analyse.scopes.scope_iterator;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(DeferStatementAst);
use(spp::analyse::scopes, class Scope);
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

  /// Check running the expression at an exit of its scope: its
  /// memory check, against the state the exit is reached with,
  /// walking the expression where it is written. Whatever it
  /// consumes is consumed there.
  auto CheckAtExit(Ast const &exit_point, StrView exit_what, ScopeManager &sm, CompilerMetaData *meta) -> void;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

private:
  /// Where stage 8 walked the expression: its scope, and the walk
  /// just before the expression's own scopes. Each exit replays
  /// the walk from here.
  analyse::scopes::Scope *_DeferScope = nullptr;
  std::optional<analyse::scopes::ScopeIterator> _DeferPosition;
};
