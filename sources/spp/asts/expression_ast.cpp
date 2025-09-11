#include <spp/asts/expression_ast.hpp>


spp::asts::ExpressionAst::ExpressionAst(ExpressionAst const &) = default;


auto spp::asts::ExpressionAst::equals_array_literal_explicit_elements(
    ArrayLiteralExplicitElementsAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_array_literal_repeated_elements(
    ArrayLiteralRepeatedElementAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_boolean_literal(
    BooleanLiteralAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_float_literal(
    FloatLiteralAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_identifier(
    IdentifierAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_integer_literal(
    IntegerLiteralAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_string_literal(
    StringLiteralAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_tuple_literal(
    TupleLiteralAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_type_identifier(
    TypeIdentifierAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_type_unary_expression(
    TypeUnaryExpressionAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals_type_postfix_expression(
    TypePostfixExpressionAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::ExpressionAst::equals(
    ExpressionAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}
