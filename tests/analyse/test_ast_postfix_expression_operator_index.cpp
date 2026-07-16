#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_on_non_indexable_type,
    SppIdentifierUnknownError, R"(
    fun f() -> Void {
        123[0]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_ref, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexRef[StrView, S32] {
        cor index_ref(&self, index: S32) -> Indexed[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x = a[0]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_mut, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexMut[StrView, S32] {
        cor index_mut(&mut self, index: S32) -> Indexed[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> Void {
        let x = a[mut 0]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_mut_on_ref_only,
    SppIdentifierUnknownError, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexRef[StrView, S32] {
        cor index_ref(&self, index: S32) -> Indexed[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x = a[mut 0]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_ref_on_mut_only,
    SppIdentifierUnknownError, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexMut[StrView, S32] {
        cor index_mut(&mut self, index: S32) -> Indexed[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> Void {
        let x = a[0]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_ref_infer_type, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexRef[StrView, S32] {
        cor index_ref(&self, index: S32) -> Indexed[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x: &StrView = a[0]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_mut_infer_type, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexMut[StrView, S32] {
        cor index_mut(&mut self, index: S32) -> Indexed[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> Void {
        let x: &mut StrView = a[mut 0]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_mut_on_immutable_receiver,
    SppInvalidMutationError, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexMut[StrView, S32] {
        cor index_mut(&mut self, index: S32) -> Indexed[&mut StrView] {
            gen &mut "1"
        }
    }

    fun f(a: A) -> Void {
        let x = a[mut 0]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_argument_type_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    use std::iterator::Indexed
    cls A { }

    sup A ext std::ops::idx::IndexRef[StrView, S32] {
        cor index_ref(&self, index: S32) -> Indexed[&StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> Void {
        let x = a["hello"]
    }
)");
