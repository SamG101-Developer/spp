#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_bad_target_1,
    SppInvalidPrimaryExpressionError, R"(
    fun g() -> Void {
        async Bool
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_bad_target_2,
    SppAsyncTargetNotFunctionCallError, R"(
    fun g() -> Void {
        async 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_moving_pinned_borrow,
    SppMovingEscapingBorrowedMemoryError, R"(
    fun f(a: &Str) -> StrView { ret "hello" }
    fun g() -> Void {
        let x = Str::from("hello")
        let future = async f(&x)
        let y = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_moving_future_with_pins_ret,
    SppMovingEscapingBorrowedMemoryError, R"(
    fun f(a: &Str) -> StrView { ret "hello" }
    fun g() -> Fut[StrView] {
        let x = Str::from("hello")
        let future = async f(&x)
        ret future
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_invalid_async_postfix_not_a_call,
    SppAsyncTargetNotFunctionCallError, R"(
    cls A { x: Bool }
    fun g() -> Void {
        let a = A(x=true)
        async a.x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_valid_async_method_call, R"(
    cls A { }
    sup A {
        fun method(&self) -> StrView { ret "hello" }
    }
    fun g() -> Void {
        let a = A()
        let mut x = async a.method()
        x = Fut[StrView]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_valid_async_good_target, R"(
    fun f() -> StrView { ret "hello" }
    fun g() -> Void {
        let mut x = async f()
        x = Fut[StrView]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUnaryExpressionOperatorAsyncAst,
    test_valid_async_good_target_with_args, R"(
    fun f(a: &StrView) -> Void { }
    fun g() -> Void {
        let mut x = async f(&"hello")
        x = Fut[Void]()
    }
)");
