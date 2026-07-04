#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_element_ast,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = [Bool]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_different_element_types,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let a = [1, false, 3]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_different_element_types_by_generic,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let a = [[1], [2, 3]]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_immutably_borrowed_elements,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &Str) -> Void {
        let b = [a]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_invalid_mutably_borrowed_elements,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &mut Str) -> Void {
        let b = [a]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_valid_1_element, R"(
    fun f() -> Void {
        let a = [1]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_valid_n_elements, R"(
    fun f() -> Void {
        let a = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_valid_nested_array_elements, R"(
    fun f() -> Void {
        let a = [[1, 2], [3, 4]]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralExplicitElementsAst,
    test_valid_nested_tuple_elements, R"(
    fun f() -> Void {
        let a = [(1, 2), (3, 4)]
    }
)");
