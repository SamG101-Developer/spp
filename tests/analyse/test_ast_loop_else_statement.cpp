#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopControlFlowStatementAst,
    test_invalid_exit_expr,
    SppExpressionTypeInvalidError, R"(
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
    fun f() -> std::void::Void {
        loop true {
            loop true {
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
        loop true {
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
    fun f() -> std::void::Void {
        loop true {
            loop true {
                exit skip
            }
            skip
        }
    }
)");



SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopControlFlowStatementAst,
    test_valid_exit_types_nested, R"(
    fun f() -> std::void::Void {
        loop true {
            loop true {
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
