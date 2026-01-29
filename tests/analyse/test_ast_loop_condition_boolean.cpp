#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_invalid_expression,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        loop std::boolean::Bool { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionBooleanAst,
    test_invalid_non_boolean_type,
    SppExpressionNotBooleanError, R"(
    fun f() -> std::void::Void {
        loop 1 { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionBooleanAst,
    test_valid_simple, R"(
    fun f() -> std::void::Void {
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
