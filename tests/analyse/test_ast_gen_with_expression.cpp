#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_ref_vs_mut,
    SppTypeMismatchError, R"(
    cor foo() -> Gen[&S32] {
        gen &1
    }

    cor bar() -> Gen[&mut S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_mut_vs_mov,
    SppTypeMismatchError, R"(
    cor foo() -> Gen[&mut S32] {
        gen &mut 1
    }

    cor bar() -> Gen[S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_mov_vs_ref,
    SppTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen 1
    }

    cor bar() -> Gen[&S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_mov_vs_mut,
    SppTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen 1
    }

    cor bar() -> Gen[&mut S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_mismatch_mut_vs_ref_coerce,
    R"(
    cor foo() -> Gen[&mut S32] {
        gen &mut 1
    }

    cor bar() -> Gen[&S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_mut_coercion,
    R"(
    cor foo() -> Gen[&mut S32] {
        gen &mut 1
    }

    cor bar() -> Gen[&S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_match_ref,
    R"(
    cor foo() -> Gen[&S32] {
        gen &1
    }

    cor bar() -> Gen[&S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_match_mut,
    R"(
    cor foo() -> Gen[&mut S32] {
        gen &mut 1
    }

    cor bar() -> Gen[&mut S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_match_mov,
    R"(
    cor foo() -> Gen[S32] {
        gen 1
    }

    cor bar() -> Gen[S32] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_type_mismatch,
    SppTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen 1
    }

    cor bar() -> Gen[Bool] {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_gen_with_in_subroutine,
    SppFunctionSubroutineContainsGenExpressionError, R"(
    cor foo() -> Gen[S32] {
        gen 1
    }

    fun bar() -> Void {
        gen with foo()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_gen_with_non_generator,
    SppExpressionNotGeneratorError, R"(
    cor bar() -> Gen[S32] {
        gen with 5
    }
)");
