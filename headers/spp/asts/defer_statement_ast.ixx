module;
#include <spp/macros.hpp>

export module spp.asts.defer_statement_ast;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(DeferStatementAst) {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

/**
 * The DeferStatementAst holds an expression to run when the scope it is written in is left, by whichever path leaves
 * it: falling off the end, a @c ret - including the one the @c "?" operator generates - or a loop @c exit or @c skip.
 */
SPP_EXP_CLS struct spp::asts::DeferStatementAst final : StatementAst {
  SPP_AST_KEY_FUNCTIONS(DeferStatementAst);

  /**
   * The @c defer token that starts this statement.
   */
  Unique<TokenAst> TokDefer;

  /**
   * The expression to run when the enclosing scope is left. It produces no value - nothing is in a position to receive
   * one - so it must be @c Void typed.
   */
  Unique<ExpressionAst> Expr;

  /**
   * Construct the DeferStatementAst with the arguments matching the members.
   * @param tok_defer The @c defer token that starts this statement.
   * @param expr The expression to run when the enclosing scope is left.
   */
  DeferStatementAst(
    decltype(TokDefer) &&tok_defer,
    decltype(Expr) &&expr);

  ~DeferStatementAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  Vec<Shared<IdentifierAst>> Consumed;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;
};
