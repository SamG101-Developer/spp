module;
#include <spp/macros.hpp>

export module spp.asts.type_parenthesised_expression_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeParenthesisedExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::TypeParenthesisedExpressionAst final : Ast, mixins::TempTypeAst {
  SPP_AST_KEY_FUNCTIONS(TypeParenthesisedExpressionAst);

  /// The "(" token that starts the parenthesised expression.
  Unique<TokenAst> TokL;

  /// The type expression enclosed within the parentheses.
  Shared<TypeAst> Expr;

  /// The ")" token that ends the parenthesised expression.
  Unique<TokenAst> TokR;

  TypeParenthesisedExpressionAst(
    decltype(TokL) &&tok_l,
    decltype(Expr) expr,
    decltype(TokR) &&tok_r);

  ~TypeParenthesisedExpressionAst() override;

  auto Convert() -> Unique<TypeAst> override;
};
