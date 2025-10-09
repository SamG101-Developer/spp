#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_2_args, R"(
    fun f() -> std::void::Void {
        let a = 1 + 2
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_3_args, R"(
    fun f() -> std::void::Void {
        let a = 1 + 2 + 3
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_comparison_collapse, R"(
    fun f() -> std::void::Void {
        let a = 1 < 2 < 3
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_lhs_folding, R"(
    fun f(b: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt, std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        let a = .. + b
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_rhs_folding, R"(
    fun f(a: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt, std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        let b = a + ..
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_lhs_value,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let a = std::bignum::bigint::BigInt + 2
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_rhs_value,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let a = 1 + std::bignum::bigint::BigInt
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_non_tuple_lhs_folding,
    SppMemberAccessNonIndexableError, R"(
    fun f(b: std::bignum::bigint::BigInt) -> std::void::Void {
        let a = .. + b
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_non_tuple_rhs_folding,
    SppMemberAccessNonIndexableError, R"(
    fun f(a: std::bignum::bigint::BigInt) -> std::void::Void {
        let b = a + ..
    }
)")

