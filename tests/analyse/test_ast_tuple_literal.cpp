#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleLiteralNElementAst,
    test_valid_tuple_filled_tuple_literal_size_1, R"(
    fun f() -> std::void::Void {
        let a = (1,)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleLiteralNElementAst,
    test_valid_tuple_filled_tuple_literal_size_n, R"(
    fun f() -> std::void::Void {
        let a = (1, 2, 3)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleLiteralNElementAst,
    test_invalid_tuple_filled_tuple_literal_invalid_element,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let a = (std::boolean::Bool, std::boolean::Bool)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleLiteralNElementAst,
    test_invalid_tuple_filled_tuple_borrowed_elements,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &std::bignum::bigint::BigInt) -> std::void::Void {
        let a = (a, 2, 3)
    }
)");
