module;
#include <spp/macros.hpp>

export module spp.asts.ret_statement_ast;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(RetStatementAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::RetStatementAst final : StatementAst {
  SPP_AST_KEY_FUNCTIONS(RetStatementAst);

  /// The "ret" token that starts this statement.
  Unique<TokenAst> TokRet;

  /// The optional value being returned from the function.
  Unique<ExpressionAst> Expr;

  struct {
    Shared<TypeAst> _OriginalRetType;
  } Source;

  RetStatementAst(
    decltype(TokRet) &&tok_ret,
    decltype(Expr) &&val);

  ~RetStatementAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

private:
  Shared<TypeAst> _RetType;
};
