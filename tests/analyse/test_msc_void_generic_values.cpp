#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_generic_call_result_passed_on_at_void, R"(
    fun take[T](t: T) -> T { ret t }

    fun relay[T, F: std::function::FunMov[(), T]](f: F) -> T {
        ret take(f())
    }

    fun f() -> Void {
        relay(() -> Void { })
        let n = relay(() -> S32 { ret 1 })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_returning_call_as_an_argument, R"(
    fun nothing() -> Void { }
    fun take[T](t: T) -> T { ret t }

    fun f() -> Void {
        take(nothing())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_the_same_instantiation_reached_twice, R"(
    fun nothing() -> Void { }
    fun take[T](t: T) -> T { ret t }

    fun f() -> Void {
        take(nothing())
        take(nothing())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_argument_built_from_a_call_with_its_own_arguments, R"(
    fun bump(n: &mut S32) -> Void { n@ += 1 }
    fun take[T](t: T) -> T { ret t }

    fun f() -> Void {
        let mut n = 0
        take(bump(&mut n))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_argument_consumes_what_it_was_built_from, R"(
    fun sink(s: Str) -> Void {
        std::mem::ops::drop(s)
    }

    fun take[T](t: T) -> T { ret t }

    fun f() -> Void {
        take(sink(Str::from("x")))
    }
)");
