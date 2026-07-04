#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_expression_in_subroutine,
    SppFunctionSubroutineContainsGenExpressionError, R"(
    fun foo() -> Void {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_mut,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_ref_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[&S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[&mut S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[&mut S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_type_mismatch,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut_coercion,
    R"(
    cor foo() -> Gen[&S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mov,
    R"(
    cor foo() -> Gen[S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_ref,
    R"(
    cor foo() -> Gen[&S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut,
    R"(
    cor foo() -> Gen[&mut S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_empty_gen_void,
    R"(
    cor foo() -> Gen[Void] {
        gen
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_gen_mut_borrow_of_mutable_symbol,
    R"(
    cor foo(mut x: S32) -> Gen[&mut S32] {
        gen &mut x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_mut_borrow_of_immutable_symbol,
    SppInvalidMutationError, R"(
    cor foo(x: S32) -> Gen[&mut S32] {
        gen &mut x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_moved_symbol_reused,
    SppUninitializedMemoryUseError, R"(
    cor foo(x: Str) -> Gen[Str] {
        gen x
        gen x
    }
)");
