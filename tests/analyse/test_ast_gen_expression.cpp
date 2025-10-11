#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_expression_in_subroutine,
    SppFunctionSubroutineContainsGenExpressionError, R"(
    fun foo() -> std::void::Void {
        gen 1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen &1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_mut,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen &mut 1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_ref_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen 1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen 1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_type_mismatch,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen false
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut_coercion,
    R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &mut 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mov,
    R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_ref,
    R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut,
    R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_yield_none_to_gen_opt, R"(
    cor c() -> std::generator::GenOpt[&std::number::S32] {
        gen
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_yield_error_to_gen_res, R"(
    cor c() -> std::generator::GenRes[&std::number::S32, Err=std::string::Str] {
        gen "error"
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_yield_error_to_gen_res_wrong_type,
    SppYieldedTypeMismatchError, R"(
    cor c() -> std::generator::GenRes[&std::number::S32, Err=std::string::Str] {
        gen 123
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_yield_none_to_non_optional_gen,
    SppYieldedTypeMismatchError, R"(
    cor c() -> std::generator::GenRes[&std::number::S32, Err=std::string::Str] {
        gen
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_yield_error_to_non_fallible_gen,
    SppYieldedTypeMismatchError, R"(
    cor c() -> std::generator::GenOpt[&std::number::S32] {
        gen "error"
    }
)")
