#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SubroutinePrototypeAst,
    test_invalid_subroutine_missing_ret_statement,
    SppFunctionSubroutineMissingReturnStatementError, R"(
    fun c() -> std::number::S32 {
        let x = 123
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SubroutinePrototypeAst,
    test_valid_subroutine_valid_no_ret_statement_void, R"(
    fun c() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SubroutinePrototypeAst,
    test_valid_subroutine_valid_ret_statement_void, R"(
    fun c() -> std::void::Void {
        ret
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SubroutinePrototypeAst,
    test_valid_subroutine_valid_ret_statement_non_void, R"(
    fun c() -> std::number::S32 {
        ret 123
    }
)");
