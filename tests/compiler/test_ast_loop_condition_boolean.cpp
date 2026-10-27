#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_invalid_expression,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        loop Bool { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_non_boolean_type,
    SppExpressionNotBooleanError, R"(
    fun f() -> Void {
        loop 1 { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_simple, R"(
    fun f() -> Void {
        let x = false
        loop x { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_never_return, R"(
    fun f() -> ! {
        loop true { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_else_block_matching_type, R"(
    fun f() -> Void {
        let mut x = loop false { exit "hello" } else { "goodbye" }
        x = "world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_else_block_mismatched_type,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x = loop false { exit "hello" } else { 123 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_else_block_after_iterations, R"(
    fun f() -> U32 {
        let mut i = 0_u32
        ret loop i < 10_u32 {
            case i == 20_u32 { exit i }
            i += 1_u32
        } else { 999_u32 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_value_loop_missing_else,
    SppLoopMissingElseError, R"(
    fun f() -> U32 {
        let mut i = 0_u32
        ret loop i < 10_u32 {
            case i == 5_u32 { exit i }
            i += 1_u32
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_value_loop_true_without_else, R"(
    fun f() -> U32 {
        ret loop true { exit 5_u32 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_void_loop_without_else, R"(
    fun f() -> Void {
        let mut i = 0_u32
        loop i < 10_u32 {
            case i == 5_u32 { exit }
            i += 1_u32
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_value_loop_statement_discarded,
    SppDiscardedValueError, R"(
    fun f() -> Void {
        let mut i = 0_u32
        loop i < 10_u32 {
            case i == 5_u32 { exit i }
            i += 1_u32
        }
    }
)");
