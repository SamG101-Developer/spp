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

    sup A ext std::ops::idx::SliceRef[StrView, S32] {
        cor slice_ref(&self, from: S32, into: S32) -> GenOnce[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x = a[0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_mut, R"(
    cls A { }

    sup A ext std::ops::idx::SliceMut[StrView, S32] {
        cor slice_mut(&mut self, from: S32, into: S32) -> GenOnce[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> Void {
        let x = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_ref_infer_type, R"(
    cls A { }

    sup A ext std::ops::idx::SliceRef[StrView, S32] {
        cor slice_ref(&self, from: S32, into: S32) -> GenOnce[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x: &StrView = a[0 to 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_valid_slicing_mut_infer_type, R"(
    cls A { }

    sup A ext std::ops::idx::SliceMut[StrView, S32] {
        cor slice_mut(&mut self, from: S32, into: S32) -> GenOnce[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> Void {
        let x: &mut StrView = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_mut_on_ref_only,
    SppIdentifierUnknownError, R"(
    cls A { }

    sup A ext std::ops::idx::SliceRef[StrView, S32] {
        cor slice_ref(&self, from: S32, into: S32) -> GenOnce[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_ref_on_mut_only,
    SppIdentifierUnknownError, R"(
    cls A { }

    sup A ext std::ops::idx::SliceMut[StrView, S32] {
        cor slice_mut(&mut self, from: S32, into: S32) -> GenOnce[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> Void {
        let x = a[0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_mut_on_immutable_receiver,
    SppInvalidMutationError, R"(
    cls A { }

    sup A ext std::ops::idx::SliceMut[StrView, S32] {
        cor slice_mut(&mut self, from: S32, into: S32) -> GenOnce[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(a: A) -> Void {
        let x = a[mut 0 to 2]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorSliceAst,
    test_invalid_slicing_bound_type_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    sup A ext std::ops::idx::SliceRef[StrView, S32] {
        cor slice_ref(&self, from: S32, into: S32) -> GenOnce[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x = a["hello" to 2]
    }
)");
