#include "../test_macros.hpp"

// Type and comp generics are handled alike: a comp pack ("cmp ..ns") is matched, spread, keyed and inferred as a type
// pack ("..Ts") is. Each comp test sits beside its type mirror where one exists.

// A pack is keyed by its elements, so an instance written with one reads each element through its bindings: "(n, 1_uz)"
// with "n" bound to "1_uz" is "(1_uz, 1_uz)", and the two calls below are two types.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_comp_pack_keyed_by_its_elements, R"(
    cls A[cmp ..ns: USize] { }
    fun f[cmp n: USize]() -> A[n, 1_uz] { ret A[n, 1_uz]() }
    fun g() -> Void {
        let a: A[1_uz, 1_uz] = f[1_uz]()
        let b: A[2_uz, 1_uz] = f[2_uz]()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

// A bound pack named among other arguments spreads into its elements.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_comp_pack_spread_among_other_arguments, R"(
    cls A[cmp ..ns: USize] { }
    fun f[cmp ..ns: USize]() -> A[0_uz, ns] { ret A[0_uz, ns]() }
    fun g() -> Void {
        let a: A[0_uz, 1_uz, 2_uz] = f[1_uz, 2_uz]()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_type_pack_spread_among_other_arguments, R"(
    cls A[..Ts] { }
    fun f[..Ts]() -> A[S32, Ts] { ret A[S32, Ts]() }
    fun g() -> Void {
        let a: A[S32, Bool, U8] = f[Bool, U8]()
        std::mem::ops::drop(a)
    }
)");

// A sup block over a pack attaches to every instance, of either kind.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_sup_over_a_comp_pack, R"(
    cls A[cmp ..n: Bool] { }
    sup [cmp ..n: Bool] A[n] {
        !public fun f(&self) -> Void { }
    }
    fun g() -> Void {
        let a = A[true, false]()
        a.f()
        std::mem::ops::drop(a)
    }
)");

// A fixed head then a pack: the pack takes what is left, of either kind.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_sup_over_a_fixed_comp_head_and_a_comp_pack, R"(
    cls A[cmp ..n: USize] { }
    sup [cmp m: USize, cmp ..rest: USize] A[m, rest] {
        !public fun f(&self) -> USize { ret m }
    }
    fun g() -> Void {
        let a = A[1_uz, 2_uz, 3_uz]()
        let x = a.f()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_sup_over_a_fixed_comp_head_and_an_empty_comp_pack, R"(
    cls A[cmp ..n: Bool] { }
    sup [cmp first: Bool, cmp ..rest: Bool] A[first, rest] {
        !public fun f(&self) -> Bool { ret first }
    }
    fun g() -> Void {
        let a = A[true]()
        let x = a.f()
        std::mem::ops::drop(a)
    }
)");

// A pack pattern with a value ahead of it matches element by element.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_sup_over_a_comp_value_then_a_comp_pack, R"(
    cls A[cmp ..ns: USize] { }
    sup [cmp ..ns: USize] A[0_uz, ns] { !public fun m(&self) -> Void { } }
    fun g() -> Void {
        let a = A[0_uz, 1_uz, 2_uz]()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeCompSymmetry,
    test_invalid_sup_over_a_comp_value_then_a_comp_pack_not_matched,
    SppIdentifierUnknownError, R"(
    cls A[cmp ..ns: USize] { }
    sup [cmp ..ns: USize] A[0_uz, ns] { !public fun m(&self) -> Void { } }
    fun g() -> Void {
        let a = A[1_uz, 2_uz]()
        a.m()
        std::mem::ops::drop(a)
    }
)");

// A pack of the other kind matches nothing.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeCompSymmetry,
    test_invalid_type_pack_sup_over_a_comp_pack_class,
    SppIdentifierUnknownError, R"(
    cls A[..Ts] { }
    sup [..Ts] A[Ts] {
        !public fun f(&self) -> Void { }
    }
    cls B[cmp ..n: Bool] { }
    fun g() -> Void {
        let b = B[true, false]()
        b.f()
    }
)");

// A type and a comp pack side by side.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_sup_over_a_type_then_a_comp_pack, R"(
    cls E[T, cmp ..n: T] { }
    sup [T, cmp ..n: T] E[T, n] {
        !public fun f(&self) -> Void { }
    }
    fun g() -> Void {
        let e = E[Bool, true, false]()
        e.f()
        std::mem::ops::drop(e)
    }
)");

// Inference reads a recorded pack element by element, for either kind.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_infer_a_comp_head_from_a_recorded_comp_pack, R"(
    cls A[cmp ..ns: USize] { }
    fun first[cmp f: USize, cmp ..r: USize](a: A[f, r]) -> USize {
        std::mem::ops::drop(a)
        ret f
    }
    fun g() -> Void { let x = first(A[1_uz, 2_uz, 3_uz]()) }
)");

// A comp-generic abstract method is implemented by an extension, as a type-generic one is.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_comp_generic_abstract_method_implemented, R"(
    cls Shape[cmp n: USize] { }
    sup [cmp n: USize] Shape[n] {
        !public
        !abstract_method
        fun area(&self, a: Arr[S32, n]) -> S32 { }
    }
    cls Sq { }
    sup Sq ext Shape[4_uz] {
        fun area(&self, a: Arr[S32, 4_uz]) -> S32 { ret 0 }
    }
    fun g() -> Void {
        let s = Sq()
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_type_generic_abstract_method_implemented, R"(
    cls Shape[T] { }
    sup [T] Shape[T] {
        !public
        !abstract_method
        fun area(&self, a: T) -> S32 { }
    }
    cls Sq { }
    sup Sq ext Shape[S32] {
        fun area(&self, a: S32) -> S32 { ret 0 }
    }
    fun g() -> Void {
        let s = Sq()
        std::mem::ops::drop(s)
    }
)");

// A comp operator expression as an argument is analysed on a copy (analysing it desugars it), and the memory check
// looks at the same thing: it crashed on the desugared call it never made.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeCompSymmetry,
    test_valid_comp_operator_argument_with_a_defaulted_comp_parameter, R"(
    fun g[cmp n: USize, cmp m: USize = n + 1_uz]() -> USize { ret m }
    fun h[cmp n: USize]() -> USize { ret g[n + 5_uz]() }
    fun k() -> Void { let x = h[1_uz]() }
)");
