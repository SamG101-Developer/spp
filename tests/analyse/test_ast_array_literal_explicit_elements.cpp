#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_element_ast,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let a = [std::boolean::Bool, std::boolean::Bool]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_different_element_types,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let a = [1, false, 3]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_borrowed_elements,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &std::bignum::bigint::BigInt) -> std::void::Void {
        let b = [a]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_mixed_borrowed_elements,
    SppTypeMismatchError, R"(
    fun f(a: &std::bignum::bigint::BigInt) -> std::void::Void {
        let b = [a, 1, 2]
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_valid_elements, R"(
    fun f() -> std::void::Void {
        let a = [1, 2, 3]
    }
)");
