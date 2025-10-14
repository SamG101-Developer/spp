#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_bad_target_1,
    SppExpressionTypeInvalidError, R"(
    fun g() -> std::void::Void {
        async std::boolean::Bool
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_bad_target_2,
    SppAsyncTargetNotFunctionCallError, R"(
    fun g() -> std::void::Void {
        async 123
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_moving_pinned_borrow,
    SppMoveFromPinnedMemoryError, R"(
    fun f(a: &std::string::Str) -> std::string::Str { ret "hello" }
    fun g() -> std::void::Void {
        let x = "hello"
        let future = async f(&x)
        let y = x
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_moving_future_with_pins_mov,
    SppMoveFromPinLinkedMemoryError, R"(
    fun f(a: &std::string::Str) -> std::string::Str { ret "hello" }
    fun g() -> std::void::Void {
        let x = "hello"
        let future = async f(&x)
        let f = future
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_moving_future_with_pins_ret,
    SppMoveFromPinLinkedMemoryError, R"(
    fun f(a: &std::string::Str) -> std::string::Str { ret "hello" }
    fun g() -> std::future::Fut[std::string::Str] {
        let x = "hello"
        let future = async f(&x)
        ret future
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_valid_async_good_target, R"(
    fun f() -> std::string::Str { ret "hello" }
    fun g() -> std::void::Void {
        let mut x = async f()
        x = std::future::Fut[std::string::Str]()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_valid_async_good_target_with_args, R"(
    fun f(a: &std::string::Str) -> std::void::Void { }
    fun g() -> std::void::Void {
        let mut x = async f(&"hello")
        x = std::future::Fut[std::void::Void]()
    }
)")
