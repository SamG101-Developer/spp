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
  SppCompTimeConstantError, R"(
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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_size_case_expression, R"(
    fun f() -> Void {
        let a = [false; case true { 2_uz } else { 3_uz }]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_size_case_expression_then_scoped_code, R"(
    fun f(p: Str) -> Void {
        let a = [false; case true { 2_uz } else { 3_uz }]
        case true {
            let q = p
            drop(q)
        }
        else {
            drop(p)
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_invalid_move_after_size_case_expression,
  SppUninitializedMemoryUseError, R"(
    fun f(p: Str) -> Void {
        let a = [false; case true { 2_uz } else { 3_uz }]
        case true {
            drop(p)
            drop(p)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_array_used_after_size_case_expression_and_scoped_code, R"(
    fun f() -> Void {
        let a = [1_s32; case true { 2_uz } else { 3_uz }]
        case true {
            let s = "hello"
        }
        let b = a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_size_comp_generic_instantiated, R"(
    fun f[cmp n: USize]() -> Void {
        let a = [false; n]
    }

    fun g() -> Void {
        f[4_uz]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_size_comp_generic_expression_instantiated, R"(
    fun f[cmp n: USize]() -> Void {
        let a = [false; n + 1_uz]
        let b: Arr[Bool, n + 1_uz] = a
    }

    fun g() -> Void {
        f[4_uz]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_invalid_size_comp_generic_expression_mismatch,
  SppTypeMismatchError, R"(
    fun f[cmp n: USize]() -> Void {
        let a = [false; n + 1_uz]
        let b: Arr[Bool, n] = a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_size_comp_generic_returned, R"(
    fun f[cmp n: USize]() -> Arr[Bool, n] {
        ret [false; n]
    }

    fun g() -> Void {
        let a: Arr[Bool, 3_uz] = f[3_uz]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_invalid_size_comp_generic_returned_mismatch,
  SppTypeMismatchError, R"(
    fun f[cmp n: USize]() -> Arr[Bool, n] {
        ret [false; n]
    }

    fun g() -> Void {
        let a: Arr[Bool, 4_uz] = f[3_uz]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_valid_size_case_expression_on_comp_generic_instantiated, R"(
    fun f[cmp n: USize]() -> Void {
        let a = [false; case n == 1_uz { 2_uz } else { n }]
        let b = a
    }

    fun g() -> Void {
        f[4_uz]()
        f[1_uz]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ArrayLiteralRepeatedElementAst,
  test_invalid_size_case_expression_on_comp_generic_against_a_concrete_type,
  SppTypeMismatchError, R"(
    fun f[cmp n: USize]() -> Void {
        let a = [false; case n == 1_uz { 2_uz } else { n }]
        let b: Arr[Bool, 2_uz] = a
    }

    fun g() -> Void {
        f[4_uz]()
    }
)");
