module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;

PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;

auto PostfixExpressionOperatorAst::ExprParts() const -> Vec<IdentifierAst*> {
  return {};
}

auto PostfixExpressionOperatorAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &) const -> Unique<PostfixExpressionOperatorAst> {
  // Default implementation that the non-specialized
  // postfix expression operators will use.
  return AstClone(this);
}

SPP_MOD_END
