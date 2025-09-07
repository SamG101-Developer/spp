#include <spp/asts/expression_ast.hpp>


spp::asts::ExpressionAst::ExpressionAst(ExpressionAst const &) = default;


auto spp::asts::ExpressionAst::equals_array_literal_explicit_elements(
    ArrayLiteralExplicitElementsAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_array_literal_repeated_elements(
    ArrayLiteralRepeatedElementAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_boolean_literal(
    BooleanLiteralAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_float_literal(
    FloatLiteralAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_identifier(
    IdentifierAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_integer_literal(
    IntegerLiteralAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_string_literal(
    StringLiteralAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_tuple_literal(
    TupleLiteralAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_type_identifier(
    TypeIdentifierAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_type_unary_expression(
    TypeUnaryExpressionAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals_type_postfix_expression(
    TypePostfixExpressionAst const &) const
    -> bool {
    return false;
}


auto spp::asts::ExpressionAst::equals(
    ExpressionAst const &) const
    -> bool {
    return false;
}
