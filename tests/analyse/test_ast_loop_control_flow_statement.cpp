#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_exit_expr,
  SppInvalidPrimaryExpressionError, R"(
    fun f() -> std::void::Void {
        loop true {
            exit std::boolean::Bool
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_too_many_control_statements,
  SppLoopTooManyControlFlowStatementsError, R"(
    fun f() -> std::void::Void {
        loop true {
            exit exit
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_exit_types_1,
  SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        loop true {
            case false of {
                == true { exit 1 }
                == false { exit true }
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_exit_types_2,
  SppTypeMismatchError, R"(
    fun f(b: std::boolean::Bool) -> std::void::Void {
        loop true {
            loop b {
                exit exit 1
            }
            exit true
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exit_types, R"(
    fun f() -> std::void::Void {
        let looped = loop true {
            case true {
                exit 1
            }
            else {
                exit 2
            }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exit_skip, R"(
    fun f(b: std::boolean::Bool) -> std::void::Void {
        loop true {
            loop b {
                exit skip
            }
            skip
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exit_types_nested, R"(
    fun f(b: std::boolean::Bool) -> std::void::Void {
        let looped = loop true {
            loop b {
                exit exit 1
            }
            exit 1
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exit_types_assigned, R"(
    fun f() -> std::void::Void {
        let mut x = loop true {
            exit "hello"
        }
        x = "goodbye"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_loop_exit_value_passed_straight_to_a_call, R"(
    fun h(n: S32) -> Void { }

    fun f() -> Void {
        let mut i = 0
        h(loop true {
            i += 1
            case i == 3 { exit i * 10 }
        })
    }
)");

// "exit" and "skip" outside any loop dereferenced the (null) current loop when building the error.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopControlFlowStatementAst,
    test_invalid_exit_outside_a_loop,
    SppLoopTooManyControlFlowStatementsError, R"(
    fun f() -> Void {
        exit
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopControlFlowStatementAst,
    test_invalid_skip_outside_a_loop,
    SppLoopTooManyControlFlowStatementsError, R"(
    fun f() -> Void {
        skip
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopControlFlowStatementAst,
    test_invalid_exit_with_value_outside_a_loop,
    SppLoopTooManyControlFlowStatementsError, R"(
    fun f() -> S32 {
        exit 5
    }
)");

// A closure body is a function of its own, so a loop around the closure is not one its "exit" can leave. It used to
// be accepted, and branched into the enclosing function's loop-end block.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopControlFlowStatementAst,
    test_invalid_exit_in_closure_targeting_the_outer_loop,
    SppLoopTooManyControlFlowStatementsError, R"(
    fun f() -> Void {
        loop true {
            let c = () { exit }
            exit
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopControlFlowStatementAst,
    test_valid_exit_in_closure_targeting_its_own_loop, R"(
    fun f() -> Void {
        let c = () {
            loop true { exit }
        }
        c()
    }
)");
