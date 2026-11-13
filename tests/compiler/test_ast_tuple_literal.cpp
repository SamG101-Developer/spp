#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_tuple_filled_tuple_literal_size_1, R"(
    fun f() -> Void {
        let a = (1,)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_tuple_filled_tuple_literal_size_n, R"(
    fun f() -> Void {
        let a = (1, 2, 3)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTupleLiteralAst,
  test_invalid_tuple_filled_tuple_literal_invalid_element,
  SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = (Bool, Bool)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTupleLiteralAst,
  test_invalid_tuple_filled_tuple_borrowed_elements,
  SppSecondClassBorrowViolationError, R"(
    fun f(a: &std::bignum::bigint::BigInt) -> Void {
        let a = (a, 2, 3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_empty_tuple_literal, R"(
    fun f() -> Void {
        let a = ()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_nested_tuple_literal, R"(
    fun f() -> Void {
        let a = ((1, 2), 3)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTupleLiteralAst,
  test_invalid_tuple_nested_invalid_element,
  SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = ((Bool, 1), 3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_tuple_literal_argument_consumes_its_elements, R"(
    fun f(p: Str, q: Str) -> Void {
        drop((p, q))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTupleLiteralAst,
  test_invalid_use_of_an_element_moved_into_a_tuple_literal_argument,
  SppUninitializedMemoryUseError, R"(
    fun f(p: Str, q: Str) -> Void {
        drop((p, q))
        drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_empty_tuple_literal_then_a_statement, R"(
    fun f() -> Void {
        let a = ()
        std::mem::ops::drop(a)
    }
)");

// A "case" giving a value as an element takes the element's type, not the type of the tuple around it (open-bugs #61).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_case_expression_as_first_element_of_a_let, R"(
    fun f(c: Bool) -> Void {
        let a = (case c { false } else { true }, 1_u8)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_case_expression_as_first_element_of_a_return, R"(
    fun f(c: Bool) -> (Bool, U8) {
        ret (case c { false } else { true }, 1_u8)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_case_expression_as_middle_element, R"(
    fun f(c: Bool) -> (U8, U8, U8) {
        ret (1_u8, case c { 2_u8 } else { 3_u8 }, 4_u8)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_case_expression_as_last_element, R"(
    fun f(c: Bool) -> (U8, Bool) {
        ret (1_u8, case c { false } else { true })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTupleLiteralAst,
  test_valid_case_expression_in_a_nested_tuple, R"(
    fun f(c: Bool) -> Void {
        let a = ((case c { 1_s32 } else { 2_s32 }, true), 3_u64)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTupleLiteralAst,
  test_invalid_case_expression_element_branches_disagree,
  SppTypeMismatchError, R"(
    fun f(c: Bool) -> Void {
        let a = (case c { false } else { 1_u8 }, 1_u8)
    }
)");
