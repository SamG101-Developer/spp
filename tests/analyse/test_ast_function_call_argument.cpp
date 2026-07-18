#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_unnamed_argument,
    SppInvalidPrimaryExpressionError, R"(
    fun f(a: Bool) -> Void { }

    fun g() -> Void {
        f(Bool)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_named_argument,
    SppInvalidPrimaryExpressionError, R"(
    fun f(a: Bool) -> Void { }

    fun g() -> Void {
        f(a=Bool)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_mut_borrow_from_ref_borrow,
    SppInvalidMutationError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_mut_borrow_from_immutable_value,
    SppInvalidMutationError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mut_vs_mov_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Bool) -> Void { }

    fun g(mut a: Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mut_vs_mov_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Bool) -> Void { }

    fun g(a: &mut Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mut_vs_mov_3,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Bool) -> Void { }

    fun g(mut a: &mut Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mov_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(&a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mov_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mistmatch_ref_vs_mov_3,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(&a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mov_vs_mut_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mut_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(&a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mut_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_ref_vs_mut_3,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(&a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_convention_mismatch_mov_vs_ref_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: &Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_mismatch_mut_ceorce_ref_1,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(mut a: Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_mismatch_mut_ceorce_ref_2,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(a: &mut Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_mismatch_mut_ceorce_ref_3,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(mut a: &mut Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mov_1,
    R"(
    fun f(a: Bool) -> Void { }

    fun g() -> Void {
        f(true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mov_2,
    R"(
    fun f(a: Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_ref_1,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_ref_2,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(&a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_ref_3,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(a: &Bool) -> Void {
        f(&a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mut_1,
    R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: &mut Bool) -> Void {
        f(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mut_2,
    R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(mut a: Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_convention_match_mut_3,
    R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(mut a: &mut Bool) -> Void {
        f(&mut a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_named_arg,
    R"(
    fun f(a: Bool) -> Void { }

    fun g() -> Void {
        f(a=true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_named_arg_with_ref_convention,
    R"(
    fun f(a: &Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(a=&a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentAst,
    test_valid_named_arg_with_mut_convention,
    R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(mut a: Bool) -> Void {
        f(a=&mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentAst,
    test_invalid_named_arg_mut_convention_from_immutable,
    SppInvalidMutationError, R"(
    fun f(a: &mut Bool) -> Void { }

    fun g(a: Bool) -> Void {
        f(a=&mut a)
    }
)");
