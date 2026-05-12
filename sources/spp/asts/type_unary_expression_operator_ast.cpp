module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_ast;

SPP_MOD_BEGIN
spp::asts::TypeUnaryExpressionOperatorAst::~TypeUnaryExpressionOperatorAst() = default;

auto spp::asts::TypeUnaryExpressionOperatorAst::operator<=>(
    TypeUnaryExpressionOperatorAst const &that) const
    -> Ordering {
    return Equals(that);
}

auto spp::asts::TypeUnaryExpressionOperatorAst::operator==(
    TypeUnaryExpressionOperatorAst const &that) const
    -> bool {
    return Equals(that) == Ordering::equal;
}

auto spp::asts::TypeUnaryExpressionOperatorAst::EqualsOpBorrow(
    TypeUnaryExpressionOperatorBorrowAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::TypeUnaryExpressionOperatorAst::EqualsOpNamespace(
    TypeUnaryExpressionOperatorNamespaceAst const &) const
    -> Ordering {
    return Ordering::less;
}

SPP_MOD_END
