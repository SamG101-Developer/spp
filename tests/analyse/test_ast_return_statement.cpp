#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_statement_in_coroutine_with_expr,
    SppCoroutineContainsReturnStatementError, R"(
    cor f() -> Gen[S32] {
        ret 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_statement_type_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> S32 {
        ret "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_statement_in_coroutine_no_expr, R"(
    cor f() -> Gen[S32] {
        ret # stops any more elements being yielded into the coroutine
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_statement_in_subroutine_no_expr, R"(
    fun f() -> std::void::Void {
        ret
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_statement_in_subroutine_with_expr, R"(
    fun f() -> S32 {
        ret 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_type_as_value,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> S32 {
        ret S32
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_void_value,
    SppInvalidVoidValueError, R"(
    fun g() -> Void { }

    fun f() -> Void {
        ret g()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstReturnStatementAst,
    test_invalid_ret_moved_value,
    SppUninitializedMemoryUseError, R"(
    fun f(x: Str) -> Str {
        let y = x
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstReturnStatementAst,
    test_valid_ret_self_type, R"(
    cls A { }

    sup A {
        fun make() -> Self {
            ret A()
        }
    }
)");
