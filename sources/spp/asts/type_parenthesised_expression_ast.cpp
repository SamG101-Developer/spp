module;
#include <spp/macros.hpp>

module spp.asts.type_parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
TypeParenthesisedExpressionAst::TypeParenthesisedExpressionAst(
  decltype(TokL) &&tok_l,
  decltype(Expr) expr,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Expr(std::move(expr)),
  TokR(std::move(tok_r)) {
}

TypeParenthesisedExpressionAst::~TypeParenthesisedExpressionAst() = default;

auto TypeParenthesisedExpressionAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL->PosStart();
}

auto TypeParenthesisedExpressionAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR->PosEnd();
}

auto TypeParenthesisedExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypeParenthesisedExpressionAst>(
    AstClone(TokL), AstCloneShared(Expr), AstClone(TokR));
}

auto TypeParenthesisedExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto TypeParenthesisedExpressionAst::Convert() -> Unique<TypeAst> {
  return AstClone(Expr);
}

SPP_MOD_END
