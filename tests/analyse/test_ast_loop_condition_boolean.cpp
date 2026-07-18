#include "../test_macros.hpp"

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
