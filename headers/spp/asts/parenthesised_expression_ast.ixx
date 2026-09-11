module;
#include <spp/macros.hpp>

export module spp.asts.parenthesised_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ParenthesisedExpressionAst) {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::ParenthesisedExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(ParenthesisedExpressionAst);

  /**
   * The @c ( token that indicates the start of a parenthesised expression.
   */
  Unique<TokenAst> TokL;

  /**
   * The expression that is enclosed in parentheses.
   */
  Unique<ExpressionAst> Expr;

  /**
   * The @c ) token that indicates the end of a parenthesised expression.
   */
  Unique<TokenAst> TokR;

  /**
   * Construct the ParenthesisedExpressionAst with the arguments matching the members.
   * @param[in] tok_open_paren The @c ( token that indicates the start of a parenthesised expression.
   * @param[in] expr The expression that is enclosed in parentheses.
   * @param[in] tok_close_paren The @c ) token that indicates the end of a parenthesised expression.
   */
  explicit ParenthesisedExpressionAst(
    decltype(TokL) &&tok_open_paren,
    decltype(Expr) &&expr,
    decltype(TokR) &&tok_close_paren);

  ~ParenthesisedExpressionAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};
