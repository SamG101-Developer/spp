#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_non_generator_object,
    SppExpressionNotGeneratorError, R"(
    fun g() -> Str {
        ret "hello"
    }

    fun f() -> Void {
        let a = g()
        let b = a.res()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_mismatch_send_type,
    SppFunctionCallNoValidSignaturesError, R"(
    cor g() -> Gen[Yield=S32, Send=Str] {
        gen 1
    }

    fun f(c: S32) -> Void {
        let a = g()
        let b = a.res(123)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_missing_send_val,
    SppFunctionCallNoValidSignaturesError, R"(
    cor g() -> Gen[Yield=S32, Send=Str] {
        gen 1
    }

    fun f(c: S32) -> Void {
        let a = g()
        let b = a.res()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_extra_send_val,
    SppFunctionCallNoValidSignaturesError, R"(
    cor g() -> Gen[Yield=S32, Send=Void] {
        gen 1
    }

    fun f(c: S32) -> Void {
        let a = g()
        let b = a.res(123)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_res_generator_object_mov, R"(
    cor g() -> Gen[Yield=S32, Send=StrView] {
        gen 1
    }

    fun f(mut c: S32) -> Void {
        let mut a = g()
        let mut b = a.res("123")
        c = case b of {
            is S32(..) { b }
            else { c }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_res_generator_object_ref, R"(
    cor g() -> Gen[Yield=&S32, Send=StrView] {
        gen &1
    }

    fun f(mut c: &S32) -> Void {
        let mut a = g()
        let mut b = a.res("123")
        c = case b of {
            is &S32(..) { b }
            else { c }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_res_generator_object_mut, R"(
    cor g() -> Gen[Yield=&mut S32, Send=StrView] {
        gen &mut 1
    }

    fun f(mut c: &mut S32) -> Void {
        let mut a = g()
        let mut b = a.res("123")
        c = case b of {
            is &mut S32(..) { b }
            else { c }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_res_void_send_type, R"(
    cor g() -> Gen[Yield=S32] {
        gen 1
        gen 2
    }

    fun f() -> Void {
        let mut a = g()
        let b = a.res()
        let c = a.res()
        let d = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_res_result_is_yield_type, R"(
    cor g() -> Gen[Yield=S32, Send=StrView] {
        gen 1
    }

    fun f() -> Void {
        let mut a = g()
        let b: S32 = a.res("123")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_result_type_mismatch,
    SppTypeMismatchError, R"(
    cor g() -> Gen[Yield=S32, Send=StrView] {
        gen 1
    }

    fun f() -> Void {
        let mut a = g()
        let b: StrView = a.res("123")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_on_immutable_generator,
    SppInvalidMutationError, R"(
    cor g() -> Gen[Yield=S32] {
        gen 1
    }

    fun f() -> Void {
        let a = g()
        let b = a.res()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_res_send_value_used_after_move,
    SppUninitializedMemoryUseError, R"(
    cor g() -> Gen[Yield=S32, Send=Str] {
        gen 1
    }

    fun f(s: Str) -> Void {
        let mut a = g()
        let x = a.res(s)
        let y = a.res(s)
    }
)");
