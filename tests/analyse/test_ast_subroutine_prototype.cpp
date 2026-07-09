#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_invalid_subroutine_missing_ret_statement,
    SppFunctionSubroutineMissingReturnStatementError, R"(
    fun c() -> S32 {
        let x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_valid_subroutine_valid_no_ret_statement_void, R"(
    fun c() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_valid_subroutine_valid_ret_statement_void, R"(
    fun c() -> Void {
        ret
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_valid_subroutine_valid_ret_statement_non_void, R"(
    fun c() -> S32 {
        ret 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_valid_subroutine_no_ret_never_body_loop, R"(
    fun c() -> S32 {
        loop true { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_valid_subroutine_no_ret_never_body_call, R"(
    fun g() -> ! {
        loop true { }
    }

    fun c() -> S32 {
        g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSubroutinePrototypeAst,
    test_valid_subroutine_abstract_method_no_ret, R"(
    cls A { }

    sup A {
        !abstract_method
        fun f(&self) -> S32 { }
    }
)");
