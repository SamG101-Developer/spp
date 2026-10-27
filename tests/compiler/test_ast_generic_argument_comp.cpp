#include "test_macros.hpp"

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

// A "cmp fun" call is a comp-time value anywhere one is: a generic argument (positional or keyword), a comp
// parameter's default, an array type's size. Each is compared with the type spelled with the literal it folds to, so
// it has to be evaluated, not only accepted. A call to a runtime function there is not a comp-time value (E3).

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_cmp_fun_call_as_a_positional_comp_argument, R"(
    cls A[cmp n: USize] { }
    cmp fun ct() -> USize { ret 3_uz }

    fun g() -> Void {
        let x: A[3_uz] = A[ct()]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_cmp_fun_call_as_a_keyword_comp_argument, R"(
    cls A[cmp n: USize] { }
    cmp fun ct() -> USize { ret 3_uz }

    fun g() -> Void {
        let x: A[3_uz] = A[n=ct()]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_cmp_fun_call_with_an_argument_as_a_comp_argument, R"(
    cls A[cmp n: USize] { }
    cmp fun twice(x: USize) -> USize { ret x * 2_uz }

    fun g() -> Void {
        let x: A[4_uz] = A[twice(2_uz)]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_cmp_fun_call_as_a_comp_parameter_default, R"(
    cmp fun ct() -> USize { ret 3_uz }
    cls B[cmp n: USize = ct()] { }

    fun g() -> Void {
        let y: B[3_uz] = B()
        std::mem::ops::drop(y)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_cmp_fun_call_as_an_array_type_size, R"(
    cmp fun ct() -> USize { ret 3_uz }

    fun g() -> Void {
        let a: [S32; ct()] = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_runtime_call_as_a_positional_comp_argument,
    SppCompTimeConstantError, R"(
    cls A[cmp n: USize] { }
    fun rt() -> USize { ret 3_uz }

    fun g() -> Void {
        let x = A[rt()]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_runtime_call_as_a_keyword_comp_argument,
    SppCompTimeConstantError, R"(
    cls A[cmp n: USize] { }
    fun rt() -> USize { ret 3_uz }

    fun g() -> Void {
        let x = A[n=rt()]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_runtime_call_as_a_comp_parameter_default,
    SppCompTimeConstantError, R"(
    fun rt() -> USize { ret 3_uz }
    cls B[cmp n: USize = rt()] { }

    fun g() -> Void {
        let y = B()
        std::mem::ops::drop(y)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_runtime_call_as_an_array_type_size,
    SppCompTimeConstantError, R"(
    fun rt() -> USize { ret 3_uz }

    fun g() -> Void {
        let a: [S32; rt()] = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_division_by_zero,
    SppDivisionByZeroError, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x = A[7_uz / 0_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_remainder_by_zero,
    SppDivisionByZeroError, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x = A[7_uz % 0_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_float_division_by_zero,
    SppDivisionByZeroError, R"(
    cls A[cmp n: F32] { }

    fun g() -> Void {
        let x = A[1.0_f32 / 0.0_f32]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_shift_too_wide,
    SppShiftAmountOutOfBoundsError, R"(
    cls A[cmp n: U8] { }

    fun g() -> Void {
        let x = A[1_u8 << 9_u32]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_comp_expression_arg_division_by_zero_once_bound,
    SppDivisionByZeroError, R"(
    cls A[cmp n: USize] { }

    fun f[cmp m: USize]() -> Void {
        let x = A[m / 0_uz]()
        std::mem::ops::drop(x)
    }

    fun g() -> Void {
        f[4_uz]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_float_remainder, R"(
    cls A[cmp n: F32] { }

    fun g() -> Void {
        let x: A[1.0_f32] = A[5.0_f32 % 2.0_f32]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_expression_arg_bool_bitwise, R"(
    cls A[cmp b: Bool] { }

    fun g() -> Void {
        let x: A[true] = A[true ^ false]()
        let y: A[true] = A[true | false]()
        let z: A[false] = A[true & false]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        std::mem::ops::drop(z)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_cmp_fun_bool_ops_match_comp_expression_arg, R"(
    cls A[cmp b: Bool] { }
    cmp fun eq() -> Bool { ret true == false }
    cmp fun xor() -> Bool { ret true ^ false }

    fun g() -> Void {
        let x: A[false == true] = A[eq()]()
        let y: A[true ^ false] = A[xor()]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// Todo (red): a comp argument is analysed with no target type ("GenericArgumentAst::AnalyseCompVal"), so a variant
//  array value holds every element to its first one's type ("None" against "Some[U64]"), not to the parameter's.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_variant_array_literal_as_a_comp_argument, R"(
    cls A[cmp n: [Opt[U64]; 2_uz]] { }

    fun g() -> Void {
        let x = A[[Some(val=1_u64), None]]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_variant_array_constant_as_a_comp_argument_read_again, R"(
    cls A[cmp n: [Opt[U64]; 2_uz]] { }
    cmp k: [Opt[U64]; 2_uz] = [Some(val=1_u64), None]

    fun g() -> Void {
        let x = A[k]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_variant_array_cmp_fun_call_as_a_comp_argument, R"(
    cls A[cmp n: [Opt[U64]; 2_uz]] { }
    cmp fun mk() -> [Opt[U64]; 2_uz] { ret [Some(val=1_u64), None] }

    fun g() -> Void {
        let x = A[mk()]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_variant_array_literal_as_a_function_comp_argument, R"(
    fun f[cmp n: [Opt[U64]; 2_uz]]() -> Void { }

    fun g() -> Void {
        f[[Some(val=1_u64), None]]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_variant_array_keyword_function_comp_argument, R"(
    fun f[cmp n: [Opt[U64]; 2_uz]]() -> Void { }

    fun g() -> Void {
        f[n=[Some(val=1_u64), None]]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_array_comp_argument_element_not_of_the_parameter_type,
    SppTypeMismatchError, R"(
    cls A[cmp n: [Opt[U64]; 2_uz]] { }

    fun g() -> Void {
        let x = A[[Some(val=1_u64), 5_u64]]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestGenericArgumentComp,
    test_invalid_array_function_comp_argument_element_not_of_the_parameter_type,
    SppTypeMismatchError, R"(
    fun f[cmp n: [Opt[U64]; 2_uz]]() -> Void { }

    fun g() -> Void {
        f[[Some(val=1_u64), 5_u64]]()
    }
)");
