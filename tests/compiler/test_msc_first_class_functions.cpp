#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_subroutine_lambda_as_value, R"(
    fun g(x: FunRef[(), StrView]) -> Void {
        let mut y = x()
        y = "Goodbye, World!"
    }

    fun f() -> Void {
        let p = () "string"
        g(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_subroutine_function_as_value, R"(
    fun h() -> StrView {
        ret "Hello, World!"
    }

    fun g(x: FunRef[(), StrView]) -> Void {
        let mut y = x()
        y = "Goodbye, World!"
    }

    fun f() -> Void {
        let p = h
        g(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_named_function_to_funref_variable_no_params, R"(
    fun h() -> StrView { ret "Hello, World!" }

    fun f() -> Void {
        let x: FunRef[(), StrView] = h
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_named_function_to_funref_variable_with_params, R"(
    fun h(a: S32) -> StrView { ret "Hello, World!" }

    fun f() -> Void {
        let x: FunRef[(S32,), StrView] = h
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_named_function_to_funref_variable_then_call, R"(
    fun h() -> StrView { ret "Hello, World!" }

    fun f() -> Void {
        let x: FunRef[(), StrView] = h
        let mut y = x()
        y = "Goodbye, World!"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_named_function_as_funref_return_type, R"(
    fun h() -> StrView { ret "Hello, World!" }

    fun get() -> FunRef[(), StrView] { ret h }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_named_function_in_funref_array, R"(
    fun h() -> StrView { ret "Hello, World!" }

    fun f() -> Void {
        let arr: [FunRef[(), StrView]; 1_uz] = [h]
    }
)");

// The correct overload must be selected by matching the target `FunRef` type against each overload's
// mock supertype.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_overloaded_function_selected_by_funref_variable_type, R"(
    fun h() -> StrView { ret "no-args" }
    fun h(a: S32) -> StrView { ret "one-arg" }

    fun f() -> Void {
        let x: FunRef[(), StrView] = h
        let y: FunRef[(S32,), StrView] = h
    }
)");

// A named function bound to a variable and then passed on should keep working through the equality
// check (distinct from passing the bare name, which already works).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    DISABLED_test_named_function_via_typed_variable_as_argument, R"(
    fun h() -> StrView { ret "Hello, World!" }

    fun g(x: FunRef[(), StrView]) -> Void {
        let mut y = x()
        y = "Goodbye, World!"
    }

    fun f() -> Void {
        let x: FunRef[(), StrView] = h
        g(x)
    }
)");

// A closure borrowed as a concrete function type is that type's fat pointer: it is called through the borrow.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    test_valid_closure_borrowed_as_fun_ref, R"(
    fun take(c: &FunRef[(), U32]) -> U32 {
        ret c()
    }

    fun f() -> Void {
        let x = 1_u32
        let c = (caps &x) { ret x + 1_u32 }
        let y = take(&c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    test_valid_closure_mutably_borrowed_as_fun_mut, R"(
    fun run(c: &mut FunMut[(), Void]) -> Void {
        c()
        c()
    }

    fun f() -> Void {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        run(&mut c)
        std::mem::ops::drop(c)
    }
)");

// The convention is still the parameter's: an immutable borrow cannot stand for a "&mut".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestFirstClassFunctions,
    test_invalid_closure_borrowed_as_mut_fun_mut,
    SppFunctionCallNoValidSignaturesError, R"(
    fun run(c: &mut FunMut[(), Void]) -> Void { }

    fun f() -> Void {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        run(&c)
    }
)");

// The borrowed closure carries its captures into the call, beside another way to the same place.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestFirstClassFunctions,
    test_invalid_closure_borrowed_as_fun_mut_beside_its_capture,
    SppMemoryOverlapUsageError, R"(
    fun run(c: &mut FunMut[(), Void], y: &mut S32) -> Void { }

    fun f() -> Void {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        run(&mut c, &mut x)
    }
)");

// A borrowed "FunRef" is called as a "FunRef", not as the "FunMut" it extends (which needed a "&mut").
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    test_valid_call_through_borrowed_fun_ref, R"(
    fun take(c: &FunRef[(), U32]) -> U32 {
        ret c()
    }
)");
