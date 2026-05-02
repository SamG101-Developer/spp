module;
#include <spp/macros.hpp>

module spp.asts.expression_ast;

SPP_MOD_BEGIN
spp::asts::ExpressionAst::~ExpressionAst() = default;

auto spp::asts::ExpressionAst::operator<=>(
    const ExpressionAst &rhs_expr) const
    -> Ordering {
    return Equals(rhs_expr);
}

auto spp::asts::ExpressionAst::operator==(
    const ExpressionAst &rhs_expr) const
    -> bool {
    return Equals(rhs_expr) == Ordering::equal;
}

auto spp::asts::ExpressionAst::EqualsArrayLiteralExplicitElements(
    ArrayLiteralExplicitElementsAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsArrayLiteralRepeatedElement(
    ArrayLiteralRepeatedElementAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsBooleanLiteral(
    BooleanLiteralAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsCharLiteral(
    CharLiteralAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsFloatLiteral(
    FloatLiteralAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsIdentifier(
    IdentifierAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsIntegerLiteral(
    IntegerLiteralAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsStringLiteral(
    StringLiteralAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTupleLiteral(
    TupleLiteralAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTypeIdentifier(
    TypeIdentifierAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTypeUnaryExpression(
    TypeUnaryExpressionAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTypePostfixExpression(
    TypePostfixExpressionAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::Equals(
    ExpressionAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::ExpressionAst::ExprParts() const
    -> Vec<Ast*> {
    return {};
}

SPP_MOD_END
