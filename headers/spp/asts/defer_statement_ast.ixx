module;
#include <spp/macros.hpp>

export module spp.asts.defer_statement_ast;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct DeferStatementAst;
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
  SPP_GCC_VTABLE_FIX
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

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  Vec<Shared<IdentifierAst>> Consumed;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::DeferStatementAst)
