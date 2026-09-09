#include "../test_macros.hpp"

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
