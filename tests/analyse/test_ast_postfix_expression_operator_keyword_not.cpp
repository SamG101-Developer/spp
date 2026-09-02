#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_valid_not_on_a_boolean_literal, R"(
    fun f() -> Void {
        let a = true.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_valid_not_on_a_boolean_variable, R"(
    fun f() -> Void {
        let a = false
        let b = a.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_valid_not_on_a_comparison, R"(
    fun f() -> Void {
        let a = (1 == 2).not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_valid_not_applied_twice, R"(
    fun f() -> Void {
        let a = true.not.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_valid_not_on_a_dereferenced_borrow, R"(
    fun g(a: &Bool) -> Bool {
        ret a@.not
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_valid_not_in_a_case_condition, R"(
    fun f() -> Void {
        case true.not { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_invalid_not_on_an_integer,
  SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        let a = 1.not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_invalid_not_on_a_non_boolean_class,
  SppExpressionNotBooleanError, R"(
    cls X { }

    fun f() -> Void {
        let a = X()
        let b = a.not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_invalid_not_on_a_borrowed_boolean,
  SppExpressionNotBooleanError, R"(
    fun g(a: &Bool) -> Void {
        let b = a.not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  PostfixExpressionOperatorKeywordNotAst,
  test_invalid_not_on_a_mutably_borrowed_boolean,
  SppExpressionNotBooleanError, R"(
    fun g(a: &mut Bool) -> Void {
        let b = a.not
    }
)");
