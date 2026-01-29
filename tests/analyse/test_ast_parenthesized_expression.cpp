#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_invalid_parenthesized_expression_invalid_expression_type,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let a = (std::boolean::Bool)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstParenthesizedExpressionAst,
    test_valid_parenthesized_expression_valid_expression_type, R"(
    fun f() -> std::void::Void {
        let mut a = (true)
        a = false
    }
)");
