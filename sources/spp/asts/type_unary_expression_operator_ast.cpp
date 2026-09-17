module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_ast;

SPP_MOD_BEGIN
TypeUnaryExpressionOperatorAst::~TypeUnaryExpressionOperatorAst() = default;

auto TypeUnaryExpressionOperatorAst::operator<=>(
  TypeUnaryExpressionOperatorAst const &that) const -> Ordering {
  return Equals(that);
}

auto TypeUnaryExpressionOperatorAst::operator==(
  TypeUnaryExpressionOperatorAst const &that) const -> bool {
  return Equals(that) == Ordering::equal;
}

auto TypeUnaryExpressionOperatorAst::EqualsOpBorrow(
  TypeUnaryExpressionOperatorBorrowAst const &) const -> Ordering {
  return Ordering::less;
}

auto TypeUnaryExpressionOperatorAst::EqualsOpNamespace(
  TypeUnaryExpressionOperatorNamespaceAst const &) const -> Ordering {
  return Ordering::less;
}

SPP_MOD_END
