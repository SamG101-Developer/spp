#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_and_over_two_booleans, R"(
    fun f() -> Void {
        let a = true and false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_or_over_two_booleans, R"(
    fun f() -> Void {
        let a = true or false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_and_chained, R"(
    fun f() -> Void {
        let a = true and false and true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_and_or_mixed, R"(
    fun f() -> Void {
        let a = true and false or true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_operands_from_comparisons, R"(
    fun f() -> Void {
        let a = 1 < 2 and 3 < 4
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_operands_from_a_comparison_chain, R"(
    fun f() -> Void {
        let a = 1 < 2 < 3 and true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_in_a_case_condition, R"(
    fun f() -> Void {
        case true and false { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_in_a_loop_condition, R"(
    fun f() -> Void {
        let mut a = true
        loop a and true { a = false }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_operands_dereferenced_from_borrows, R"(
    fun g(a: &Bool, b: &Bool) -> Bool {
        ret a@ and b@
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_valid_operand_from_a_not_expression, R"(
    fun f() -> Void {
        let a = true.not and false
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_and_integer_lhs,
  SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        let a = 1 and true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_and_integer_rhs,
  SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        let a = true and 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_or_integer_lhs,
  SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        let a = 1 or true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_or_integer_rhs,
  SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        let a = true or 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_and_borrowed_lhs,
  SppExpressionNotBooleanError, R"(
    fun g(a: &Bool) -> Void {
        let c = a and true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_and_borrowed_rhs,
  SppExpressionNotBooleanError, R"(
    fun g(b: &Bool) -> Void {
        let c = true and b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_or_borrowed_lhs,
  SppExpressionNotBooleanError, R"(
    fun g(a: &Bool) -> Void {
        let c = a or true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_and_mutably_borrowed_lhs,
  SppExpressionNotBooleanError, R"(
    fun g(a: &mut Bool) -> Void {
        let c = a and true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_uninitialized_symbol_on_the_left_of_and,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Bool
        let a = elem and true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_uninitialized_symbol_on_the_right_of_and,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Bool
        let a = true and elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  BinaryExpressionLogicalAst,
  test_invalid_uninitialized_symbol_on_the_right_of_or,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Bool
        let a = false or elem
    }
)");
