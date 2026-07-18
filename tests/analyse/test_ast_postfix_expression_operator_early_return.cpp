#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_valid_early_return_option, R"(
    fun f(o: Opt[S32]) -> Opt[S32] {
        let x = o?
        ret Some(val=x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_valid_early_return_result, R"(
    fun f(r: Res[S32, Str]) -> Res[S32, Str] {
        let x = r?
        ret Pass(val=x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_valid_early_return_on_function_call_result, R"(
    fun g() -> Opt[S32] {
        ret Some(val=1)
    }

    fun f() -> Opt[S32] {
        let x = g()?
        ret Some(val=x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_valid_early_return_result_different_value_type, R"(
    fun f(r: Res[S32, Str]) -> Res[Bool, Str] {
        let x = r?
        ret Pass(val=true)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_invalid_early_return_non_try_type,
    SppEarlyReturnRequiresTryTypeError, R"(
    fun f() -> Opt[S32] {
        let x = true?
        ret None
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_invalid_early_return_option_in_result_function,
    SppTypeMismatchError, R"(
    fun f(o: Opt[S32]) -> Res[S32, Str] {
        let x = o?
        ret Pass(val=x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_invalid_early_return_result_in_option_function,
    SppTypeMismatchError, R"(
    fun f(r: Res[S32, Str]) -> Opt[S32] {
        let x = r?
        ret Some(val=x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_invalid_early_return_result_mismatched_error_type,
    SppTypeMismatchError, R"(
    fun f(r: Res[S32, Str]) -> Res[S32, Bool] {
        let x = r?
        ret Pass(val=x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorEarlyReturnAst,
    test_invalid_early_return_in_non_try_return_function,
    SppTypeMismatchError, R"(
    fun f(o: Opt[S32]) -> S32 {
        let x = o?
        ret x
    }
)");
