#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_condition_iterable_invalid_expression,
    SppInvalidPrimaryExpressionError, R"(
    use std::iterator::Iterator
    fun f() -> Void {
        loop x in Iterator[Str] { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_condition_iterable_invalid_type,
    SppExpressionNotGeneratorError, R"(
    fun f() -> Void {
        loop x in 0 { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_assign_to_iterator,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let mut v = Vec[Str]()
        loop mut x in v.iter_mut() {
            x = Str::from("hello")
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_mut, R"(
    fun f(mut y: &mut Str) -> Void {
        let mut v = Vec[Str]()
        loop mut x in v.iter_mut() {
            x = y
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_ref, R"(
    fun f(mut y: &Str) -> Void {
        let mut v = Vec[Str]()
        loop mut x in v.iter_ref() {
            x = y
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_move, R"(
    fun f() -> Void {
        let v = Vec[Str]()
        loop mut x in v.iter_mov() {
            x = Str::from("hello")
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_simple, R"(
    fun f() -> Void {
        let v = Vec[Str]()
        loop x in v.iter_ref() { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_over_coroutine, R"(
    cor gen_strings() -> Gen[Str] {
        gen Str::from("hello")
    }

    fun f() -> Void {
        loop x in gen_strings() { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LoopConditionIterableAst,
    test_valid_loop_condition_iterable_destructure_tuple, R"(
    cor gen_pairs() -> Gen[(Bool, Bool)] {
        gen (false, false)
    }

    fun f() -> Void {
        loop (a, b) in gen_pairs() { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LoopConditionIterableAst,
    test_invalid_loop_condition_iterable_immutable_variable_assign,
    SppInvalidMutationError, R"(
    fun f(y: &mut Str) -> Void {
        let mut v = Vec[Str]()
        loop x in v.iter_mut() {
            x = y
        }
    }
)");
