#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_on_non_sliceable_type,
    SppIdentifierUnknownError, R"(
    fun f() -> Void {
        123[0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_ref, R"(
    cls A { }

    fun f(a: Vec[S32]) -> Void {
        let x = a[0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_mut, R"(
    cls A { }

    fun f(mut a: Vec[S32]) -> Void {
        let x = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_ref_infer_type, R"(
    cls A { }

    fun f(a: Vec[S32]) -> Void {
        let x: &S32 = a[0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_mut_infer_type, R"(
    cls A { }

    fun f(mut a: Vec[S32]) -> Void {
        let x: &mut S32 = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_mut_on_ref_only,
    SppIdentifierUnknownError, R"(
    cls A { }

    fun f(a: Vec[S32]) -> Void {
        let x = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_ref_on_mut_only,
    SppIdentifierUnknownError, R"(
    cls A { }

    fun f(mut a: Vec[S32]) -> Void {
        let x = a[0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_bound_type_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    fun f(a: Vec[S32]) -> Void {
        let x = a["hello" to 2]
    }
)");
