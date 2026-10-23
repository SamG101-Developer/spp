#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_ret,
  SppUnreachableCodeError, R"(
    fun f() -> S32 {
        ret 1
        ret 2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_exit,
  SppUnreachableCodeError, R"(
    fun f() -> Void {
        loop true {
            exit
            let a = 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_skip,
  SppUnreachableCodeError, R"(
    fun f() -> Void {
        loop true {
            skip
            let a = 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_a_terminating_block,
  SppUnreachableCodeError, R"(
    fun f() -> S32 {
        { ret 100 }
        ret 200
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_a_nested_terminating_block,
  SppUnreachableCodeError, R"(
    fun f() -> S32 {
        { { ret 100 } }
        ret 200
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_a_case_whose_branches_all_return,
  SppUnreachableCodeError, R"(
    fun f(c: Bool) -> S32 {
        case c { ret 1 }
        else { ret 2 }
        ret 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_statement_after_a_case_with_no_else, R"(
    fun f(c: Bool) -> S32 {
        case c { ret 1 }
        ret 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_statement_after_a_case_with_a_branch_that_falls_through, R"(
    fun f(c: Bool) -> S32 {
        case c { ret 1 }
        else { }
        ret 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_statement_after_a_conditional_loop_that_returns, R"(
    fun f(c: Bool) -> S32 {
        loop c { ret 1 }
        ret 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_ret_as_the_final_statement, R"(
    fun f() -> S32 {
        let a = 1
        ret a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_function_ending_in_a_conditional_loop_that_returns,
  SppFunctionSubroutineMissingReturnStatementError, R"(
    fun f(c: Bool) -> S32 {
        loop c { ret 1 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_an_exhaustive_case_whose_branches_all_return,
  SppUnreachableCodeError, R"(
    fun f(x: S32 or Bool) -> S32 {
        case x of {
            is S32(..) { ret 1 }
            is Bool(..) { ret 2 }
        }
        ret 3
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDeadCode,
  test_invalid_statement_after_an_exhaustive_case_whose_branches_all_diverge,
  SppUnreachableCodeError, R"(
    fun g() -> ! {
        loop true { }
    }

    fun f(x: S32 or Bool) -> S32 {
        case x of {
            is S32(..) { g() }
            is Bool(..) { g() }
        }
        ret 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_function_ending_in_an_exhaustive_case_whose_branches_all_diverge, R"(
    fun g() -> ! {
        loop true { }
    }

    fun f(x: S32 or Bool) -> S32 {
        case x of {
            is S32(..) { g() }
            is Bool(..) { g() }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_function_ending_in_an_exhaustive_case_whose_branches_bind_a_never_value, R"(
    fun g() -> ! {
        loop true { }
    }

    fun f(x: S32 or Bool) -> S32 {
        case x of {
            is S32(..) { let a = g() }
            is Bool(..) { let b = g() }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDeadCode,
  test_valid_statement_after_an_exhaustive_case_with_a_branch_that_finishes, R"(
    fun g() -> ! {
        loop true { }
    }

    fun f(x: S32 or Bool) -> S32 {
        case x of {
            is S32(..) { g() }
            is Bool(..) { }
        }
        ret 3
    }
)");
