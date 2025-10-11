#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_gen_expression_unroll,
    SppTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &1
        gen &2
        gen &3
    }

    cor bar() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 0
        gen with foo()
        gen &mut 4
    }
)")