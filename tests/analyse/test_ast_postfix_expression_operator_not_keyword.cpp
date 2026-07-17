#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_invalid_non_boolean_lhs,
    SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        123.not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_invalid_non_boolean_lhs_string,
    SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        "hello".not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_invalid_non_boolean_lhs_custom_type,
    SppExpressionNotBooleanError, R"(
    cls A { }

    fun f(a: A) -> Void {
        a.not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_invalid_result_type_is_boolean_not_lhs,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x: S32 = true.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_lhs, R"(
    fun f() -> Void {
        true.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_lhs_false, R"(
    fun f() -> Void {
        false.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_lhs_stacked, R"(
    fun f() -> Void {
        true.not.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_variable_lhs, R"(
    fun f() -> Void {
        let x = true
        x.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_boolean_function_call_lhs, R"(
    fun g() -> Bool {
        ret true
    }

    fun f() -> Void {
        g().not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_result_used_as_boolean, R"(
    fun f() -> Void {
        let x: Bool = true.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_result_used_in_loop_condition, R"(
    fun f() -> Void {
        loop false.not { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorNotKeywordAst,
    test_valid_comptime_inversion, R"(
    cmp x: Bool = true.not
)");
