#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_statement_in_coroutine_with_expr,
    SppCoroutineContainsRetExprExpressionError, R"(
    cor f() -> std::generator::Gen[std::bignum::bigint::BigInt] {
        ret 123
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_statement_type_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> std::bignum::bigint::BigInt {
        ret "hello world"
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_statement_in_coroutine_no_expr, R"(
    cor f() -> std::generator::Gen[std::bignum::bigint::BigInt] {
        ret
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_statement_in_subroutine_no_expr, R"(
    fun f() -> std::void::Void {
        ret
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_statement_in_subroutine_with_expr, R"(
    fun f() -> std::bignum::bigint::BigInt {
        ret 1
    }
)")
