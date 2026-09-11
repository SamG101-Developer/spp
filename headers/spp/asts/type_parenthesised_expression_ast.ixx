module;
#include <spp/macros.hpp>

export module spp.asts.type_parenthesised_expression_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeParenthesisedExpressionAst) {
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::TypeParenthesisedExpressionAst final : Ast, mixins::TempTypeAst {
  SPP_AST_KEY_FUNCTIONS(TypeParenthesisedExpressionAst);

  /**
   * The left parenthesis token that represents the start of the parenthesised expression.
   */
  Unique<TokenAst> TokL;

  /**
   * The type expression that is enclosed within the parentheses.
   */
  Shared<TypeAst> Expr;

  /**
   * The right parenthesis token that represents the end of the parenthesised expression.
   */
  Unique<TokenAst> TokR;

  /**
   * Construct the TypeParenthesisedExpressionAst with the arguments matching the members.
   * @param tok_l The left parenthesis token.
   * @param expr The type expression within the parentheses.
   * @param tok_r The right parenthesis token.
   */
  TypeParenthesisedExpressionAst(
    decltype(TokL) &&tok_l,
    decltype(Expr) expr,
    decltype(TokR) &&tok_r);

  ~TypeParenthesisedExpressionAst() override;

  auto Convert() -> Unique<TypeAst> override;
};
