#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_arg_matches_usize, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x = A[123_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_arg_type_mismatch,
    SppTypeMismatchError, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x = A[123]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_arg_generic_type_matches, R"(
    cls A[T, cmp n: T] { }

    fun g() -> Void {
        let x = A[USize, 123_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_arg_generic_type_mismatch,
    SppTypeMismatchError, R"(
    cls A[T, cmp n: T] { }

    fun g() -> Void {
        let x = A[USize, true]()
    }
)");

// FIXED (the test itself also left "x" unconsumed)
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_reverse_infer_type_from_comp_arg, R"(
    cls A[T, cmp n: T] { }

    fun g() -> Void {
        let x = A[n=123_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_reverse_infer_type_from_comp_arg_verified, R"(
    cls A[T, cmp n: T] { !public a: T }

    fun g() -> Void {
        let mut x = A[n=123_uz](a=0_uz)
        x.a = 456_uz
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_reverse_infer_wrong_attribute_type,
    SppTypeMismatchError, R"(
    cls A[T, cmp n: T] { a: T }

    fun g() -> Void {
        let x = A[n=123_uz](a=true)
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_reverse_infer_type_from_comp_arg_function, R"(
    fun f[T, cmp n: T]() -> T { ret T() }

    fun g() -> Void {
        let mut x = f[n=123_uz]()
        x = 456_uz
    }
)");

// Comp-to-type inference: a comp value's type binds the type parameter its parameter is declared with, wherever the
// argument is written and however it is spelled.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_type_from_comp_arg_in_a_return_type, R"(
    cls A[T, cmp n: T] { }
    fun h() -> A[n=1_uz] { ret A[n=1_uz]() }
    fun g() -> Void { std::mem::ops::drop(h()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_type_from_comp_arg_in_an_attribute_type, R"(
    cls A[T, cmp n: T] { }
    cls B { a: A[n=1_uz] }
    fun g() -> Void { std::mem::ops::drop(B()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_type_from_positional_comp_arg, R"(
    cls A[T, cmp n: T] { }
    fun g() -> Void { std::mem::ops::drop(A[123_uz]()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_type_from_folded_comp_expression_arg, R"(
    cls A[T, cmp n: T] { }
    fun g() -> Void { std::mem::ops::drop(A[n=1_uz + 2_uz]()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_type_from_variadic_comp_args, R"(
    cls V[T, cmp ..n: T] { }
    fun g() -> Void { std::mem::ops::drop(V[1_uz, 2_uz]()) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_infer_type_from_mixed_variadic_comp_args,
    SppTypeMismatchError, R"(
    cls V[T, cmp ..n: T] { }
    fun g() -> Void { std::mem::ops::drop(V[1_uz, true]()) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_arg_against_an_explicit_type,
    SppTypeMismatchError, R"(
    cls A[T, cmp n: T] { }
    fun g() -> Void { std::mem::ops::drop(A[T=Bool, n=1_uz]()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_type_from_comp_arg_flows_into_the_result, R"(
    cls A[T, cmp n: T] { }
    fun f[T, cmp n: T]() -> A[T, n] { ret A[T, n]() }
    fun g() -> Void {
        let x: A[USize, 5_uz] = f[n=5_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_infer_type_from_comp_arg_flows_into_the_wrong_result,
    SppTypeMismatchError, R"(
    cls A[T, cmp n: T] { }
    fun f[T, cmp n: T]() -> A[T, n] { ret A[T, n]() }
    fun g() -> Void {
        let x: A[U32, 5_u32] = f[n=5_uz]()
        std::mem::ops::drop(x)
    }
)");

// Type-to-comp inference: a comp parameter written into a parameter's type is bound from the argument's type, and
// from there can bind a type parameter in turn.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_comp_from_a_class_argument, R"(
    cls A2[cmp n: USize] { }
    fun f[cmp n: USize](a: A2[n]) -> USize {
        std::mem::ops::drop(a)
        ret n
    }
    fun g() -> Void { let x: USize = f(A2[3_uz]()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_comp_then_type_from_a_class_argument, R"(
    cls C[T, cmp n: T] { }
    fun f[U, cmp m: U](c: C[U, m]) -> C[U, m] { ret c }
    fun g() -> Void {
        let x: C[USize, 4_uz] = f(C[n=4_uz]())
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_infer_comp_conflicting_across_arguments,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A2[cmp n: USize] { }
    fun f[cmp n: USize](a: A2[n], b: A2[n]) -> Void {
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
    fun g() -> Void { f(A2[1_uz](), A2[2_uz]()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_infer_comp_from_an_array_size, R"(
    fun f[T, cmp n: USize](a: Arr[T, n]) -> USize {
        std::mem::ops::drop(a)
        ret n
    }
    fun g() -> Void { let x = f([1, 2, 3]) }
)");

// Checking the open "n + 1_uz" in the template's signature (stage 6) instantiated "USize::add" before the template
// resolved its own "self: Self", so the instance kept a bare "Self" and "uadd[Self]" reached "intrinsics.spp" (E26).
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_distinct_per_binding, R"(
    cls A[cmp n: USize] { }

    fun f[cmp n: USize]() -> A[n + 1_uz] { ret A[n + 1_uz]() }

    fun g() -> Void {
        let x: A[2_uz] = f[1_uz]()
        let y: A[3_uz] = f[2_uz]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_in_a_signature_with_no_call, R"(
    cls A[cmp n: USize] { }
    fun f[cmp n: USize]() -> A[n + 1_uz] { ret A[n + 1_uz]() }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_folds_literals, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x: A[4_uz] = A[2_uz + 2_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_parenthesised, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x: A[6_uz] = A[(1_uz + 2_uz) * 2_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_array_size, R"(
    fun g() -> Void {
        let a: Arr[S32, 1_uz + 2_uz] = [1, 2, 3]
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_comparison_is_bool, R"(
    cls B[cmp b: Bool] { }

    fun g() -> Void {
        let x: B[true] = B[1_uz < 2_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_value_mismatch,
    SppTypeMismatchError, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x: A[3_uz] = A[1_uz + 1_uz]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_overflow,
    SppIntegerOutOfBoundsError, R"(
    cls C[cmp n: U8] { }

    fun g() -> Void {
        let x = C[255_u8 + 1_u8]()
    }
)");
