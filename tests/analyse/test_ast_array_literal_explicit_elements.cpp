#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_explicit_elements_literal_invalid_element,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let a = [std::boolean::Bool, std::boolean::Bool]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_explicit_elements_different_types,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let a = [1, false, 3]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_explicit_elements_borrowed_elements,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &std::bignum::bigint::BigInt) -> std::void::Void {
        let b = [a]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_array_literal_explicit_elements_mixed_borrowed_elements,
    SppTypeMismatchError, R"(
    fun f(a: &std::bignum::bigint::BigInt) -> std::void::Void {
        let b = [a, 1, 2]
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_array_literal_explicit_elements, R"(
    fun f() -> std::void::Void {
        let a = [1, 2, 3]
    }
)")
