#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_invalid_parenthesized_expression_invalid_expression_type,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = (Bool)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_invalid_parenthesized_expression_nested_invalid_expression_type,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = ((Bool))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_valid_parenthesized_expression_valid_expression_type, R"(
    fun f() -> Void {
        let mut a = (true)
        a = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_valid_parenthesized_expression_nested, R"(
    fun f() -> Void {
        let mut a = ((true))
        a = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_valid_parenthesized_expression_type_inference, R"(
    fun f() -> Void {
        let mut a = (1 + 2)
        a = 5
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_valid_parenthesized_expression_precedence, R"(
    fun f() -> Void {
        let mut a = (1 + 2) * 3
        a = 5
    }
)");
