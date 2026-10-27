#include "test_macros.hpp"

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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_valued_exit_after_a_diverging_exit, R"(
    fun f(c: Bool) -> S32 {
        let x = loop true {
            case c { exit std::abort::abort() }
            exit 5
        }
        ret x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_exit_type_mismatch_after_a_diverging_exit,
  SppTypeMismatchError, R"(
    fun f(c: Bool, d: Bool) -> S32 {
        let x = loop true {
            case c { exit std::abort::abort() }
            case d { exit 5 }
            exit false
        }
        ret x
    }
)");

// A "skip" or an "exit" runs what the loop body deferred before it jumps. Generating that can generate other code (a
// std function's body, on demand), which saves and restores the loop stack, so a jump that held its target by
// reference across it branched through freed memory and crashed the compiler.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_skip_running_a_deferred_call, R"(
    fun f() -> Void {
        let mut i = 0_u32
        loop i < 2_u32 {
            defer std::console::println("x")
            i += 1_u32
            case i == 1_u32 { skip }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exit_running_a_deferred_call, R"(
    fun f() -> Void {
        let mut i = 0_u32
        loop i < 2_u32 {
            defer std::console::println("x")
            i += 1_u32
            case i == 1_u32 { exit }
        }
    }
)");

// A loop's "exit"s agree on one type as a "case"'s branches do ("type_join::TypeJoin"): the widest variant among them,
// so a variant after one of its members fits, as a member after its variant does.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exit_wider_than_the_first_exit, R"(
    fun f(b: Bool, n: Opt[S32]) -> Void {
        let x = loop true {
            case b of {
                == true { exit Some(val=1) }
                else { exit n }
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_exit_mismatching_the_first_exit, SppTypeMismatchError, R"(
    fun f(b: Bool) -> Void {
        let x = loop true {
            case b of {
                == true { exit 1_u8 }
                else { exit "a" }
            }
        }
    }
)");

// Each loop has its own exit type, so a loop after another at the same depth is not checked against its "exit".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_sibling_loops_exiting_with_different_types, R"(
    fun f() -> Void {
        let x = loop true {
            exit 1_u8
        }
        let y = loop true {
            exit "a"
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_nested_loop_exits_kept_apart_from_a_sibling, R"(
    fun f() -> Void {
        let x = loop true {
            let y = loop true {
                exit "a"
            }
            exit 1_u8
        }
        let z = loop true {
            let w = loop true {
                exit 2_u8
            }
            exit "b"
        }
    }
)");

// A loop assigned to a declared type takes it, as a "case" does, so every "exit" goes into that type whatever order
// they come in.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  LoopControlFlowStatementAst,
  test_valid_exits_into_the_declared_type_of_a_loop, R"(
    fun f(b: Bool, n: Opt[S32]) -> Void {
        let x: Opt[S32] = loop true {
            case b of {
                == true { exit Some(val=1) }
                else { exit n }
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_exit_not_fitting_the_declared_type_of_a_loop, SppTypeMismatchError, R"(
    fun f() -> Void {
        let x: Opt[S32] = loop true {
            exit "a"
        }
    }
)");

// With no "exit", the loop is still "!" whatever it is assigned to, so what follows it is unreachable.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LoopControlFlowStatementAst,
  test_invalid_code_after_a_declared_type_loop_without_exits, SppUnreachableCodeError, R"(
    fun f() -> Void {
        let x: Opt[S32] = loop true { }
        let y = 1
    }
)");
