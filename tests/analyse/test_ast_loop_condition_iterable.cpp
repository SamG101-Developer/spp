#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_condition_iterable_invalid_expression,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        loop x in std::iterator::IterMov[std::string::Str] { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_condition_iterable_invalid_type,
    SppExpressionNotGeneratorError, R"(
    fun f() -> std::void::Void {
        loop x in 0 { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_assign_to_iterator,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let mut v = std::vector::Vec[std::string::Str]()
        loop mut x in v.iter_mut() {
            x = "hello"
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_mut, R"(
    fun f(mut y: &mut std::string::Str) -> std::void::Void {
        let mut v = std::vector::Vec[std::string::Str]()
        loop x in v.iter_mut() {
            y = x
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_ref, R"(
    fun f(mut y: &std::string::Str) -> std::void::Void {
        let mut v = std::vector::Vec[std::string::Str]()
        loop x in v.iter_ref() {
            y = x
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_move, R"(
    fun f() -> std::void::Void {
        let v = std::vector::Vec[std::string::Str]()
        loop mut x in v.iter_mov() {
            x = "hello"
        }
    }
)");
