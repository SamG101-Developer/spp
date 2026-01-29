#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_invalid_non_boolean_lhs,
    SppExpressionNotBooleanError, R"(
    fun f() -> std::void::Void {
        123.not
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_lhs, R"(
    fun f() -> std::void::Void {
        true.not
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_lhs_stacked, R"(
    fun f() -> std::void::Void {
        true.not.not
    }
)");
