module;
#include <spp/macros.hpp>

export module spp.asts.gen_with_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenWithExpressionAst) {
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::GenWithExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(GenWithExpressionAst);

  /**
   * The token that represents a generation point. This is the @c gen keyword in the source code, which indicates that
   * the coroutine is suspending its execution and yielding a value.
   */
  Unique<TokenAst> TokGen;

  /**
   * The token that represents the @c with keyword in the source code. This indicates that the expression is being
   * generated with a specific context.
   */
  Unique<TokenAst> TokWith;

  /**
   * The expression that is being generated with the context. This is the value that will be used in the generation.
   */
  Unique<ExpressionAst> Expr;

  /**
   * Construct the GenWithExpressionAst with the arguments matching the members.
   * @param tok_gen The token that represents the @c gen keyword in the source code.
   * @param tok_with The token that represents the @c with keyword in the source code.
   * @param expr The expression that is being generated with the context.
   */
  GenWithExpressionAst(
    decltype(TokGen) &&tok_gen,
    decltype(TokWith) &&tok_with,
    decltype(Expr) &&expr);

  ~GenWithExpressionAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

private:
  Unique<ExpressionAst> _MappedLoop;
};
