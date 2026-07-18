#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_valid_unbound_skip_middle, R"(
    fun f() -> Void {
        let (a, .., c) = (1, 2, 3, 4, 5)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_valid_bound_skip_middle, R"(
    fun f() -> Void {
        let (a, ..mut b, c) = (1, 2, 3, 4)
        b = (5, 6)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_valid_bound_skip_start, R"(
    fun f() -> Void {
        let (..mut a, b) = (1, 2, 3, 4)
        a = (5, 6, 7)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_valid_bound_skip_end, R"(
    fun f() -> Void {
        let (a, b, ..mut c) = (1, 2, 3, 4)
        c = (5, 6)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_valid_unbound_skip_in_array, R"(
    fun f() -> Void {
        let [a, .., c] = [1, 2, 3, 4, 5]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_invalid_multiple_skips_in_tuple,
    SppMultipleRestPatternsError, R"(
    fun f() -> Void {
        let (a, .., .., b) = (1, 2, 3, 4)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureSkipMultipleArgumentsAst,
    test_invalid_bound_skip_in_object,
    SppVariableObjectDestructureWithBoundRestPatternError, R"(
    cls Point {
        !public x: std::number::S32
        !public y: std::number::S32
        !public z: std::number::S32
    }

    fun f() -> Void {
        let Point(x, ..y, z) = Point(x=1, y=2, z=3)
    }
)");
