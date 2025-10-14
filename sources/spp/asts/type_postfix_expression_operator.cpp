#include <spp/asts/type_postfix_expression_operator_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_nested_type_ast.hpp>


auto spp::asts::TypePostfixExpressionOperatorAst::operator<=>(
    TypePostfixExpressionOperatorAst const &that) const
    -> std::strong_ordering {
    return equals(that);
}


auto spp::asts::TypePostfixExpressionOperatorAst::operator==(
    TypePostfixExpressionOperatorAst const &that) const
    -> bool {
    return equals(that) == std::strong_ordering::equal;
}


auto spp::asts::TypePostfixExpressionOperatorAst::equals_nested_type(
    TypePostfixExpressionOperatorNestedTypeAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}
