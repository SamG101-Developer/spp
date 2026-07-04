#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_element_ast,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = [Bool; 1_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_immutably_borrowed_element,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &Bool) -> Void {
        let b = [a; 1_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_mutably_borrowed_element,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &mut Bool) -> Void {
        let b = [a; 1_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_non_copyable_element,
    SppNonCopyableTypeError, R"(
    fun f() -> Void {
        let a = [Str::from("hello"); 1_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_non_constant_size,
    SppCompileTimeConstantError, R"(
    fun f() -> Void {
        let b = 100_u32
        let a = [1_u32; b]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_size_ast,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = [1_u32; Bool]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_size_type,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let a = [false; 1]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_valid_size_literal, R"(
    fun f() -> Void {
        let a = [false; 1_uz]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_valid_size_constant, R"(
    fun f[cmp n: USize]() -> Void {
        let a = [false; n]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_valid_size_constant_expression, R"(
    fun f[cmp n: USize]() -> Void {
        let mut a = [false; 1_uz + 2_uz]
        a = [false, false, false]
    }
)");
