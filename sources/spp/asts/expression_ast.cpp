module;
#include <spp/macros.hpp>

module spp.asts.expression_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
ExpressionAst::ExpressionAst() = default;
ExpressionAst::~ExpressionAst() = default;

auto ExpressionAst::operator<=>(
  const ExpressionAst &rhs_expr) const -> Ordering {
  return Equals(rhs_expr);
}

auto ExpressionAst::operator==(
  const ExpressionAst &rhs_expr) const -> bool {
  return Equals(rhs_expr) == Ordering::equal;
}

auto ExpressionAst::EqualsArrayLiteralExplicitElements(
  ArrayLiteralExplicitElementsAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsArrayLiteralRepeatedElement(
  ArrayLiteralRepeatedElementAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsBooleanLiteral(
  BooleanLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsCharLiteral(
  CharLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsFloatLiteral(
  FloatLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsIdentifier(
  IdentifierAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsIntegerLiteral(
  IntegerLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsStringLiteral(
  StringLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsTupleLiteral(
  TupleLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsTypeIdentifier(
  TypeIdentifierAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsTypeUnaryExpression(
  TypeUnaryExpressionAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::EqualsTypePostfixExpression(
  TypePostfixExpressionAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::Equals(
  ExpressionAst const &) const -> Ordering {
  return Ordering::less;
}

auto ExpressionAst::ExprParts() const -> Vec<IdentifierAst*> {
  // The default "parts" list is empty, and certain asts
  // add a part.
  return {};
}

auto ExpressionAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &) const -> Shared<ExpressionAst> {
  // The default operation is to do nothing, because all
  // other asts will specialize.
  return AstCloneShared(this);
}

SPP_MOD_END
