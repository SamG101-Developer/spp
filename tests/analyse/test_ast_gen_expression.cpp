#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_expression_in_subroutine,
    SppFunctionSubroutineContainsGenExpressionError, R"(
    fun foo() -> std::void::Void {
        gen 1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen &1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_mut,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen &mut 1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_ref_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen 1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen 1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_type_mismatch,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut_coercion,
    R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &mut 1
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mov,
    R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen 1
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_ref,
    R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &1
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut,
    R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }
)");
