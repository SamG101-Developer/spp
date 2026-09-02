module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;

spp::asts::PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;

auto spp::asts::PostfixExpressionOperatorAst::ExprParts() const
  -> Vec<Ast*> {
  return {};
}

auto spp::asts::PostfixExpressionOperatorAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &) const
  -> Unique<PostfixExpressionOperatorAst> {
  // Default implementation that the non-specialized
  // postfix expression operators will use.
  return AstClone(this);
}

SPP_MOD_END
