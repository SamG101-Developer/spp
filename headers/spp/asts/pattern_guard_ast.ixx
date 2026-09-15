module;
#include <spp/macros.hpp>

export module spp.asts.pattern_guard_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PatternGuardAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::PatternGuardAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(PatternGuardAst);

  /// The "and" keyword token, introducing the pattern guard
  /// after a pattern.
  Unique<TokenAst> TokAnd;

  /// The guard expression, evaluated to determine if the
  /// pattern matches. Must be a boolean expression.
  Unique<ExpressionAst> Expr;

  PatternGuardAst(
    decltype(TokAnd) &&tok_and,
    decltype(Expr) &&expression);

  ~PatternGuardAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
