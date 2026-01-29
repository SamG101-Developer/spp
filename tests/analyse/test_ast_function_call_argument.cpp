#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_unnamed_argument,
    SppExpressionTypeInvalidError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(std::boolean::Bool)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_named_argument,
    SppExpressionTypeInvalidError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(a=std::boolean::Bool)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_mut_borrow_from_ref_borrow,
    SppInvalidMutationError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_mut_borrow_from_immutable_value,
    SppInvalidMutationError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mut_vs_mov_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mut_vs_mov_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(a: &mut std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mut_vs_mov_3,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: &mut std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mov_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(&a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mov_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mistmatch_ref_vs_mov_3,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(&a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mov_vs_mut_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mut_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(&a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mut_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mut_3,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(&a)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mov_vs_ref_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_mismatch_mut_ceorce_ref_1,
    R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_mismatch_mut_ceorce_ref_2,
    R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: &mut std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_mismatch_mut_ceorce_ref_3,
    R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: &mut std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mov_1,
    R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(true)
    }
)");



SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mov_2,
    R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_ref_1,
    R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_ref_2,
    R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: std::boolean::Bool) -> std::void::Void {
        f(&a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_ref_3,
    R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: &std::boolean::Bool) -> std::void::Void {
        f(&a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mut_1,
    R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(a: &mut std::boolean::Bool) -> std::void::Void {
        f(a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mut_2,
    R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mut_3,
    R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: &mut std::boolean::Bool) -> std::void::Void {
        f(&mut a)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_named_arg,
    R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(a=true)
    }
)");
