module;
#include <spp/macros.hpp>

module spp.asts.type_postfix_expression_operator_ast;

SPP_MOD_BEGIN
TypePostfixExpressionOperatorAst::TypePostfixExpressionOperatorAst() = default;

TypePostfixExpressionOperatorAst::~TypePostfixExpressionOperatorAst() = default;

auto TypePostfixExpressionOperatorAst::operator<=>(
  TypePostfixExpressionOperatorAst const &that) const -> Ordering {
  return Equals(that);
}

auto TypePostfixExpressionOperatorAst::operator==(
  TypePostfixExpressionOperatorAst const &that) const -> bool {
  return Equals(that) == Ordering::equal;
}

auto TypePostfixExpressionOperatorAst::EqualsNestedType(
  TypePostfixExpressionOperatorNestedTypeAst const &) const -> Ordering {
  return Ordering::less;
}

SPP_MOD_END
