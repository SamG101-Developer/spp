#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_repeated_element_ref_borrow,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void {
        let b = [a; 1_uz]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_repeated_element_mut_borrow,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void {
        let b = [a; 1_uz]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_repeated_element_non_copyable_element,
    SppDereferenceInvalidExpressionNonCopyableTypeError, R"(
    fun f() -> std::void::Void {
        let a = ["hello"; 1_uz]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_repeated_element_non_cmp_value,
    SppCompileTimeConstantError, R"(
    fun f() -> std::void::Void {
        let b = 100_u32
        let a = [1_u32; b]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_repeated_element_wrong_size_type,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let a = [false; 1]
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_array_literal_repeated_element_cmp_size_literal, R"(
    fun f() -> std::void::Void {
        let a = [false; 1_uz]
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_array_literal_repeated_element_cmp_size_var, R"(
    use std::number::USize
    fun f[cmp n: USize]() -> std::void::Void {
        let a = [false; n]
    }
)")
