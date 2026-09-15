module;
#include <spp/macros.hpp>

export module spp.asts.gen_with_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenWithExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::GenWithExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(GenWithExpressionAst);

  /// The "gen" token, marking the point where the coroutine
  /// suspends its execution and yields a value.
  Unique<TokenAst> TokGen;

  /// The "with" token, indicating the expression is being
  /// generated with a specific context.
  Unique<TokenAst> TokWith;

  /// The expression being generated with the context; the
  /// value used in the generation.
  Unique<ExpressionAst> Expr;

  GenWithExpressionAst(
    decltype(TokGen) &&tok_gen,
    decltype(TokWith) &&tok_with,
    decltype(Expr) &&expr);

  ~GenWithExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

private:
  Unique<ExpressionAst> _MappedLoop;
};
