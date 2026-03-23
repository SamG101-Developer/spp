#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction,
    R"(
    use std::bignum::bigint::BigInt
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction_use_generic_1,
    R"(
    fun f[T](a: Vec[T]) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction_use_generic_2,
    R"(
    fun f[T, cmp n: USize](a: Arr[T, n]) -> Void { }
)");
