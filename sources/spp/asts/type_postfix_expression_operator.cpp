#include <spp/asts/type_postfix_expression_operator_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_nested_type_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_optional_ast.hpp>


auto spp::asts::TypePostfixExpressionOperatorAst::equals_nested_type(
    TypePostfixExpressionOperatorNestedTypeAst const &) const
    -> bool {
    return false;
}


auto spp::asts::TypePostfixExpressionOperatorAst::equals_optional(
    TypePostfixExpressionOperatorOptionalAst const &) const
    -> bool {
    return false;
}
