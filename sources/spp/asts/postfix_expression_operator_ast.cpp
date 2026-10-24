module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_ast;
import spp.asts.expression_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;

PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;

auto PostfixExpressionOperatorAst::RebuildsOperand() const -> bool {
  // Most operators work on their analysed operand.
  return false;
}

auto PostfixExpressionOperatorAst::WalksOperand() const -> bool {
  // The expression walks the operand, unless it was
  // never analysed in place.
  return RebuildsOperand();
}

auto PostfixExpressionOperatorAst::ExprParts() const -> Vec<IdentifierAst*> {
  return {};
}

auto PostfixExpressionOperatorAst::ReadExpr(
  ExprSubst const &) const -> Unique<PostfixExpressionOperatorAst> {
  // Default implementation that the non-specialized
  // postfix expression operators will use.
  return AstClone(this);
}

SPP_MOD_END
