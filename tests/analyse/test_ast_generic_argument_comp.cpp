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

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_reverse_infer_type_from_comp_arg, R"(
    cls A[T, cmp n: T] { }

    fun g() -> Void {
        let x = A[n=123_uz]()
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

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_reverse_infer_type_from_comp_arg_function, R"(
    fun f[T, cmp n: T]() -> T { ret T() }

    fun g() -> Void {
        let mut x = f[n=123_uz]()
        x = 456_uz
    }
)");

// Todo: red - folding is done ("A[n + 1_uz]" with "n" bound to "1_uz" is "A[2_uz]"), but checking the open "n + 1_uz" in
// the template "f" (with "n" unbound) mints "USize::add"'s instantiation, and when that is drained its body's
// "uadd[Self]" reaches "intrinsics.spp" with "Self" unsubstituted (E26 "Self"). "n + 1_uz" in a function body's plain
// expression position ("ret n + 1_uz") does not hit it.
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
