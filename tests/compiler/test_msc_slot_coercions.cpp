#include "test_macros.hpp"

// Every place a value lands in a slot of a declared type goes through "codegen::CoerceToSlot": a variant member is
// tagged into the variant, and a named function is built into the function value. One test per slot and kind. These
// build and verify the IR only; what the values read back as at runtime is "tests/codegen/check-coercions.sh".

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_let, R"(
    fun f() -> Void {
        let x: Opt[S32] = Some(val=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_let, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let x: FunRef[(S32,), S32] = g
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_ret, R"(
    fun f() -> Opt[S32] {
        ret Some(val=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_ret, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> FunRef[(S32,), S32] {
        ret g
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_assignment, R"(
    fun f() -> Void {
        let mut x: Opt[S32] = None
        x = Some(val=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_assignment, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let mut x: FunRef[(S32,), S32] = g
        x = g
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_argument, R"(
    fun k(o: Opt[S32]) -> Void { }

    fun f() -> Void {
        k(Some(val=1))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_argument, R"(
    fun g(x: S32) -> S32 { ret x }

    fun k(h: FunRef[(S32,), S32]) -> S32 { ret h(1) }

    fun f() -> Void {
        let y = k(g)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_default_argument, R"(
    fun k(o: Opt[S32] = Some(val=1)) -> Void { }

    fun f() -> Void {
        k()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_attribute, R"(
    cls A {
        !public
        o: Opt[S32]
    }

    fun f() -> A {
        ret A(o=Some(val=1))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_attribute, R"(
    fun g(x: S32) -> S32 { ret x }

    cls A {
        !public
        h: FunRef[(S32,), S32]
    }

    fun f() -> A {
        ret A(h=g)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_attribute_assignment, R"(
    cls A {
        !public
        o: Opt[S32]
    }

    fun f(mut a: A) -> A {
        a.o = Some(val=1)
        ret a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_gen, R"(
    cor f() -> Gen[Opt[S32]] {
        gen Some(val=1)
        gen None
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_gen, R"(
    fun g(x: S32) -> S32 { ret x }

    cor f() -> Gen[FunRef[(S32,), S32]] {
        gen g
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_send, R"(
    cor c() -> Gen[S32, Opt[S32]] {
        let s = gen 1
    }

    fun f() -> Void {
        let mut g = c()
        let a = g.res(None)
        let b = g.res(Some(val=1))
        std::mem::ops::drop(g)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_assigned_case_branch, R"(
    fun f(b: Bool, n: Opt[S32]) -> Void {
        let x: Opt[S32] = case b of {
            == true { n }
            else { Some(val=1) }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_case_branch_as_argument, R"(
    fun k(o: Opt[S32]) -> Void { }

    fun f(b: Bool, n: Opt[S32]) -> Void {
        k(case b of {
            == true { n }
            else { Some(val=1) }
        })
    }
)");

// A "case" passed straight to a call has no target type, so its branches must agree, and distinct functions are
// distinct types. Declaring the function type ("let f: FunRef[..] = case ..") is what makes it one value.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSlotCoercions,
    test_invalid_distinct_functions_in_untyped_case,
    SppTypeMismatchError, R"(
    fun g(x: S32) -> S32 { ret x }

    fun h(x: S32) -> S32 { ret x + 1 }

    fun k(i: FunRef[(S32,), S32]) -> S32 { ret i(1) }

    fun f(b: Bool) -> Void {
        let y = k(case b of {
            == true { g }
            else { h }
        })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_distinct_functions_in_typed_case, R"(
    fun g(x: S32) -> S32 { ret x }

    fun h(x: S32) -> S32 { ret x + 1 }

    fun k(i: FunRef[(S32,), S32]) -> S32 { ret i(1) }

    fun f(b: Bool) -> Void {
        let i: FunRef[(S32,), S32] = case b of {
            == true { g }
            else { h }
        }
        let y = k(i)
    }
)");

// An argument is its parameter's value, so the "case" is not checked against "y", which takes the call's "U8".
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_case_argument_ignores_the_callers_target, R"(
    fun k(x: S32) -> U8 { ret 1_u8 }

    fun f(b: Bool) -> Void {
        let y = k(case b of {
            == true { 1 }
            else { 2 }
        })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_closure_body, R"(
    fun f() -> Void {
        let c = () -> Opt[S32] { Some(val=1) }
        std::mem::ops::drop(c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_closure_body, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let c = () -> FunRef[(S32,), S32] { g }
        std::mem::ops::drop(c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_loop_else, R"(
    fun f(c: Bool, n: Opt[S32]) -> Void {
        let x = loop c {
            exit n
        } else {
            Some(val=1)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_loop_exit, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let x: FunRef[(S32,), S32] = loop true {
            exit g
        }
    }
)");

// "exit n" makes the loop an "Opt[S32]", which a later "Some(..)" goes into.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_loop_exit, R"(
    fun f(b: Bool, n: Opt[S32]) -> Void {
        let x = loop true {
            case b of {
                == true { exit n }
                else { exit Some(val=1) }
            }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_array_element, R"(
    fun f(n: Opt[S32]) -> Void {
        let a: [Opt[S32]; 2_uz] = [n, Some(val=1)]
    }
)");

// The declared type is the literal's target, so it holds "Opt[S32]"s rather than typing itself off its first element.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_members_into_array_element, R"(
    fun f() -> Void {
        let a: [Opt[S32]; 2_uz] = [Some(val=1), None]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_distinct_functions_into_array_element, R"(
    fun g(x: S32) -> S32 { ret x }

    fun h(x: S32) -> S32 { ret x + 1 }

    fun f() -> Void {
        let a: [FunRef[(S32,), S32]; 2_uz] = [g, h]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_array_element, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let a: [FunRef[(S32,), S32]; 2_uz] = [g, g]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_repeated_array_element, R"(
    fun f() -> Void {
        let a: [Opt[S32]; 2_uz] = [Some(val=1); 2_uz]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_repeated_array_element, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let a: [FunRef[(S32,), S32]; 2_uz] = [g; 2_uz]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_tuple_element, R"(
    fun f() -> Void {
        let t: (Opt[S32], S32) = (Some(val=1), 1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_function_into_tuple_element, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let t: (FunRef[(S32,), S32], S32) = (g, 1)
    }
)");

// A "cmp" is a constant, so its variant is built in place rather than through a stack slot.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_cmp, R"(
    cmp x: Opt[S32] = Some(val=1)

    fun f() -> Opt[S32] {
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_holding_a_pointer_into_cmp, R"(
    cmp x: S32 or &StrView = "text"

    fun f() -> Void {
        case x of {
            is &StrView(..) { std::console::println(x) }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_cmp_tuple_element, R"(
    cmp x: (Opt[S32], S32) = (Some(val=1), 2)

    fun f() -> (Opt[S32], S32) {
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_cmp_array_element, R"(
    cmp x: [Opt[S32]; 2_uz] = [Some(val=1), Some(val=2)]

    fun f() -> [Opt[S32]; 2_uz] {
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_variant_member_into_static_cmp, R"(
    cls K { }

    sup K {
        !public
        cmp x: Opt[S32] = Some(val=1)
    }

    fun f() -> Opt[S32] {
        ret K::x
    }
)");

// A named function's mock stands for that one function and carries nothing at runtime, so another function of the
// same signature does not fit a variable holding one: calling "x" would still call "g".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSlotCoercions,
    test_invalid_assigning_one_function_to_another_functions_variable,
    SppTypeMismatchError, R"(
    fun g(x: S32) -> S32 { ret x }

    fun h(x: S32) -> S32 { ret x + 1 }

    fun f() -> Void {
        let mut x = g
        x = h
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSlotCoercions,
    test_invalid_closure_into_a_functions_variable,
    SppTypeMismatchError, R"(
    fun g(x: S32) -> S32 { ret x }

    fun f() -> Void {
        let mut x = g
        x = (y: S32) y
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSlotCoercions,
    test_closure_into_another_closures_variable, R"(
    fun f() -> Void {
        let mut x = (y: S32) y
        x = (y: S32) y + 1
        std::mem::ops::drop(x)
    }
)");
