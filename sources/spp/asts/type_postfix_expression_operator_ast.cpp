module;
#include <spp/macros.hpp>

module spp.asts.type_postfix_expression_operator_ast;

SPP_MOD_BEGIN
spp::asts::TypePostfixExpressionOperatorAst::TypePostfixExpressionOperatorAst() = default;

spp::asts::TypePostfixExpressionOperatorAst::~TypePostfixExpressionOperatorAst() = default;

auto spp::asts::TypePostfixExpressionOperatorAst::operator<=>(
    TypePostfixExpressionOperatorAst const &that) const
    -> Ordering {
    return Equals(that);
}

auto spp::asts::TypePostfixExpressionOperatorAst::operator==(
    TypePostfixExpressionOperatorAst const &that) const
    -> bool {
    return Equals(that) == Ordering::equal;
}

auto spp::asts::TypePostfixExpressionOperatorAst::EqualsNestedType(
    TypePostfixExpressionOperatorNestedTypeAst const &) const
    -> Ordering {
    return Ordering::less;
}

SPP_MOD_END
