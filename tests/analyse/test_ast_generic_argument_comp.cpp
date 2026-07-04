#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentComp,
    test_valid_comp_arg_matches_usize, R"(
    cls A[cmp n: USize] { }

    fun g() -> Void {
        let x = A[123_uz]()
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
    cls A[T, cmp n: T] { a: T }

    fun g() -> Void {
        let mut x = A[n=123_uz](a=0_uz)
        x.a = 456_uz
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
