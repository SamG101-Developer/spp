#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_2_args, R"(
    fun f() -> Void {
        let a = 1 + 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_3_args, R"(
    fun f() -> Void {
        let a = 1 + 2 + 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_add, R"(
    fun f() -> Void {
        let a = 1 + 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_sub, R"(
    fun f() -> Void {
        let a = 1 - 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_mul, R"(
    fun f() -> Void {
        let a = 1 * 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_div, R"(
    fun f() -> Void {
        let a = 1 / 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_rem, R"(
    fun f() -> Void {
        let a = 1 % 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_pow, R"(
    fun f() -> Void {
        let a = 1 ** 2_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_bit_and, R"(
    fun f() -> Void {
        let a = 1 & 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_bit_ior, R"(
    fun f() -> Void {
        let a = 1 | 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_bit_xor, R"(
    fun f() -> Void {
        let a = 1 ^ 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_bit_shl, R"(
    fun f() -> Void {
        let a = 1 << 2_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_operation_bit_shr, R"(
    fun f() -> Void {
        let a = 1 >> 2_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_eq, R"(
    fun f() -> Void {
        let a = 1 == 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_ne, R"(
    fun f() -> Void {
        let a = 1 != 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_le, R"(
    fun f() -> Void {
        let a = 1 <= 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_gt, R"(
    fun f() -> Void {
        let a = 1 > 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_ge, R"(
    fun f() -> Void {
        let a = 1 >= 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_logical_and, R"(
    fun f() -> Void {
        let a = true and false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_logical_or, R"(
    fun f() -> Void {
        let a = true or false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_compound_assignment, R"(
    fun f() -> Void {
        let mut a = 1
        a += 2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_compound_assignment_immutable_target,
    SppInvalidMutationError, R"(
    fun f() -> Void {
        let a = 1
        a += 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_collapse, R"(
    fun f() -> Void {
        let a = 1 < 2 < 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_collapse_symbolic_middle, R"(
    fun f() -> Void {
        let x = 2
        let a = 1 < x < 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_collapse_long_chain, R"(
    fun f() -> Void {
        let a = 1 < 2 < 3 < 4
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_comparison_collapse_mixed_operators, R"(
    fun f() -> Void {
        let a = 1 < 2 <= 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_lhs_folding, R"(
    use std::bignum::bigint::BigInt
    fun f(b: (BigInt, BigInt, BigInt, BigInt)) -> Void {
        let a = .. + b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    BinaryExpressionAst,
    test_valid_binary_rhs_folding, R"(
    use std::bignum::bigint::BigInt
    fun f(a: (BigInt, BigInt, BigInt, BigInt)) -> Void {
        let b = a + ..
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_lhs_value,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let a = std::bignum::bigint::BigInt + 2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_rhs_value,
    SppInvalidPrimaryExpressionError, R"(
    use std::bignum::bigint::BigInt
    fun f() -> Void {
        let a = 1 + BigInt
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_non_tuple_lhs_folding,
    SppMemberAccessNonIndexableError, R"(
    use std::bignum::bigint::BigInt
    fun f(b: BigInt) -> Void {
        let a = .. + b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_non_tuple_rhs_folding,
    SppMemberAccessNonIndexableError, R"(
    use std::bignum::bigint::BigInt
    fun f(a: BigInt) -> Void {
        let b = a + ..
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_tuple_0_elem_lhs_folding,
    SppInvalidBinaryFoldExpressionError, R"(
    fun f(a: ()) -> Void {
        let b = .. + a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_tuple_1_elem_lhs_folding,
    SppInvalidBinaryFoldExpressionError, R"(
    fun f(a: (S32,)) -> Void {
        let b = .. + a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_tuple_0_elem_rhs_folding,
    SppInvalidBinaryFoldExpressionError, R"(
    fun f(a: ()) -> Void {
        let b = a + ..
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    BinaryExpressionAst,
    test_invalid_binary_tuple_1_elem_rhs_folding,
    SppInvalidBinaryFoldExpressionError, R"(
    fun f(a: (S32,)) -> Void {
        let b = a + ..
    }
)");
