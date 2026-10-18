#include "test_macros.hpp"

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

// A class keeps an attribute whose type is "Void" ("v: T" at "T=Void"); the layout gives it no field, so what it is
// given is generated (a call returning "Void" still runs) but stored nowhere.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_attribute_given_a_void_value, R"(
    fun nothing() -> Void { }
    cls Box[T] { !public v: T }

    fun f() -> Void {
        let b = Box[Void](v=nothing())
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_attribute_inferred_from_a_void_value, R"(
    fun nothing() -> Void { }
    cls Box[T] { !public v: T }

    fun f() -> Void {
        let b = Box(v=nothing())
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_attribute_beside_a_value, R"(
    cls Pair[A, B] {
        !public a: A
        !public b: B
    }

    fun take(p: Pair[S32, Void]) -> S32 {
        let x = p.a
        std::mem::ops::drop(p)
        ret x
    }

    fun f() -> S32 {
        ret take(Pair[S32, Void](a=1_s32))
    }
)");

// "Void"'s one value is no value: always initialized, and "Copy".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_attribute_left_out_is_initialized, R"(
    cls Box[T] { !public v: T }

    fun f() -> Void {
        let b = Box[Void]()
        let x = b.v
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_destructured_void_needs_no_consuming, R"(
    cls Box[T] { !public v: T }

    fun f() -> Void {
        let b = Box[Void]()
        let Box[Void](v) = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_unused_void_parameter_needs_no_drop, R"(
    fun takes(x: Void) -> S32 { ret 1_s32 }

    fun f() -> S32 {
        ret takes()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VoidGenericValues,
  test_valid_void_value_used_twice, R"(
    fun nothing() -> Void { }
    cls Box[T] { !public v: T }

    fun f() -> Void {
        let v = nothing()
        let a = Box(v=v)
        let b = Box(v=v)
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");
