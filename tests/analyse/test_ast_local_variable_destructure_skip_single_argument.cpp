#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipSingleArgumentAst,
    test_valid_skip_in_tuple, R"(
    fun f() -> Void {
        let (a, _, c) = (1, 2, 3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipSingleArgumentAst,
    test_valid_skip_in_array, R"(
    fun f() -> Void {
        let [a, _, c] = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipSingleArgumentAst,
    test_valid_skip_multiple_elements, R"(
    fun f() -> Void {
        let (_, _, c) = (1, 2, 3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipSingleArgumentAst,
    test_valid_skip_all_elements, R"(
    fun f() -> Void {
        let (_, _) = (1, 2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureSkipSingleArgumentAst,
    test_valid_skip_when_destructuring_parameter, R"(
    fun f(t: (S32, S32)) -> Void {
        let (a, _) = t
    }
)");
