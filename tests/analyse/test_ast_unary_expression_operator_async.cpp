#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_invalid_async_unknown_target,
  SppIdentifierUnknownError, R"(
    fun g() -> Void {
        async x()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_invalid_async_bad_target_1,
  SppInvalidPrimaryExpressionError, R"(
    fun g() -> Void {
        async Bool
    }
)")

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
    fun f(a: &StrView) -> Str { ret Str::from(a) }
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
    fun f(a: &StrView) -> Str { ret Str::from(a) }
    fun g() -> Fut[Str] {
        let x = Str::from("hello")
        let future = async f(&x)
        ret future
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_invalid_async_postfix_not_a_call,
  SppAsyncTargetNotFunctionCallError, R"(
    cls A { !public x: Bool }
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
        !public
        fun method(&self) -> Str { ret Str::from("hello") }
    }
    fun g() -> Void {
        let a = A()
        let mut x = async a.method()
        x = Fut[Str]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_valid_async_good_target, R"(
    fun f() -> Str { ret Str::from("hello") }
    fun g() -> Void {
        let mut x = async f()
        x = Fut[Str]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_valid_async_good_target_with_args, R"(
    fun f(a: &StrView) -> Void { }
    fun g() -> Void {
        let mut x = async f("hello")
        x = Fut[Void]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_valid_async_compound_argument_naming_a_local, R"(
    fun ident(n: S32) -> S32 { ret n }
    fun take(n: S32) -> S32 { ret n + 1 }
    fun g() -> Void {
        let x = 5
        let f = async take(ident(x))
        std::mem::ops::drop(f)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_valid_async_borrowed_argument_released_after_await, R"(
    fun f(a: &StrView) -> Str { ret Str::from(a) }
    fun g() -> Void {
        let x = Str::from("hello")
        let future = async f(&x)
        let s = future.await
        std::mem::ops::drop(s)
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_invalid_await_target_not_a_future,
  SppAwaitTargetNotFutureError, R"(
    fun g() -> Void {
        let x = 123
        let y = x.await
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestUnaryExpressionOperatorAsyncAst,
  test_invalid_dropping_a_future_that_holds_a_pinned_borrow,
  SppMovingEscapingBorrowedMemoryError, R"(
    fun f(a: &StrView) -> Str { ret Str::from(a) }
    fun g() -> Void {
        let x = Str::from("hello")
        let future = async f(&x)
        std::mem::ops::drop(future)
    }
)");
