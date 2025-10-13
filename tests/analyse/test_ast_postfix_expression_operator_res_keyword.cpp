#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_non_generator_object,
    SppExpressionNotGeneratorError, R"(
    fun g() -> std::string::Str {
        ret "hello"
    }

    fun f() -> std::void::Void {
        let a = g()
        let b = a.res()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_mismatch_send_type,
    SppFunctionCallNoValidSignaturesError, R"(
    cor g() -> std::generator::Gen[Yield=std::number::S32, Send=std::string::Str] {
        gen 1
    }

    fun f(c: std::number::S32) -> std::void::Void {
        let a = g()
        let b = a.res(123)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_missing_send_val,
    SppFunctionCallNoValidSignaturesError, R"(
    cor g() -> std::generator::Gen[Yield=std::number::S32, Send=std::string::Str] {
        gen 1
    }

    fun f(c: std::number::S32) -> std::void::Void {
        let a = g()
        let b = a.res()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_invalid_extra_send_val,
    SppFunctionCallNoValidSignaturesError, R"(
    cor g() -> std::generator::Gen[Yield=std::number::S32, Send=std::void::Void] {
        gen 1
    }

    fun f(c: std::number::S32) -> std::void::Void {
        let a = g()
        let b = a.res(123)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_generator_object_mov, R"(
    cor g() -> std::generator::Gen[Yield=std::number::S32, Send=std::string::Str] {
        gen 1
    }

    fun f(mut c: std::number::S32) -> std::void::Void {
        let mut a = g()
        let mut b = a.res("123")
        c = iter b of {
            value { value }
            !! { c }
        }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_generator_object_ref, R"(
    cor g() -> std::generator::Gen[Yield=&std::number::S32, Send=std::string::Str] {
        gen &1
    }

    fun f(mut c: &std::number::S32) -> std::void::Void {
        let mut a = g()
        let mut b = a.res("123")
        c = iter b of {
            value { value }
            !! { c }
        }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_generator_object_mut, R"(
    cor g() -> std::generator::Gen[Yield=&mut std::number::S32, Send=std::string::Str] {
        gen &mut 1
    }

    fun f(mut c: &mut std::number::S32) -> std::void::Void {
        let mut a = g()
        let mut b = a.res("123")
        c = iter b of {
            value { value }
            !! { c }
        }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorResumeCoroutineAst,
    test_valid_void_send_type, R"(
    cor g() -> std::generator::Gen[Yield=std::number::S32] {
        gen 1
        gen 2
    }

    fun f() -> std::void::Void {
        let mut a = g()
        let b = a.res()
        let c = a.res()
        let d = b
    }
)")
