#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_immutably_borrowed_element,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void {
        let b = [a; 1_uz]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_mutably_borrowed_element,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void {
        let b = [a; 1_uz]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_non_copyable_element,
    SppInvalidExpressionNonCopyableTypeError, R"(
    fun f() -> std::void::Void {
        let a = ["hello"; 1_uz]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_non_constant_size,
    SppCompileTimeConstantError, R"(
    fun f() -> std::void::Void {
        let b = 100_u32
        let a = [1_u32; b]
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_invalid_size_type,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let a = [false; 1]
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_valid_size_literal, R"(
    fun f() -> std::void::Void {
        let a = [false; 1_uz]
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ArrayLiteralRepeatedElementAst,
    test_valid_size_constant, R"(
    use std::number::USize
    fun f[cmp n: USize]() -> std::void::Void {
        let a = [false; n]
    }
)")
