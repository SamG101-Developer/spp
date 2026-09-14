#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_copyable_capture_leaves_the_original_usable, R"(
    fun f() -> S32 {
        let a = 7
        let g = (caps a) -> S32 { a }
        ret g() + a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_copyable_capture_used_by_two_closures, R"(
    fun f() -> S32 {
        let a = 7
        let g = (caps a) -> S32 { a }
        let h = (caps a) -> S32 { a }
        ret g() + h()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_non_copyable_capture_is_moved, R"(
    fun f() -> Str {
        let s = Str::from("x")
        let g = (caps s) -> Str { s }
        ret g()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureCaptures,
  test_invalid_use_of_a_non_copyable_value_after_capturing_it,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let s = Str::from("x")
        let g = (caps s) -> Str { s }
        std::mem::ops::drop(s)
        std::mem::ops::drop(g)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_borrowed_capture_leaves_the_original_usable, R"(
    fun f() -> S32 {
        let a = 7
        let g = (caps &a) -> S32 { a }
        ret a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_drop_a_closure_with_captures_and_no_parameters, R"(
    fun f() -> Void {
        let a = 7
        let g = (caps a) -> Void { }
        std::mem::ops::drop(g)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_drop_a_captured_closure_through_a_generic_parameter, R"(
    fun consume[F: std::function::FunMov[(), Void]](f: F) -> Void {
        std::mem::ops::drop(f)
    }

    fun f() -> Void {
        let a = 7
        consume((caps a) -> Void { })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_named_function_value_called_twice, R"(
    fun add_one(x: S32) -> S32 { ret x + 1 }

    fun f() -> Void {
        let g = add_one
        let a = g(1)
        let b = g(2)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureCaptures,
  test_invalid_fun_mov_generic_called_twice_even_for_a_named_function,
  SppUninitializedMemoryUseError, R"(
    fun add_one(x: S32) -> S32 { ret x + 1 }

    fun call_twice[F: std::function::FunMov[(S32,), S32]](f: F, x: S32) -> S32 { ret f(f(x)) }

    fun f() -> Void {
        let r = call_twice(add_one, 40)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureCaptures,
  test_valid_named_function_value_passed_on_twice, R"(
    fun add_one(x: S32) -> S32 { ret x + 1 }

    fun call_once[F: std::function::FunMov[(S32,), S32]](f: F, x: S32) -> S32 { ret f(x) }

    fun f() -> Void {
        let g = add_one
        let a = call_once(g, 1)
        let b = call_once(g, 2)
    }
)");
