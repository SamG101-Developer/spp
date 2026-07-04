#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_on_non_indexable_type,
    SppIdentifierUnknownError, R"(
    fun f() -> std::void::Void {
        123[0]
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_ref, R"(
    cls A { }

    sup A ext std::ops::idx::IndexRef[std::string_view::StrView, std::number::S32] {
        cor index_ref(&self, index: std::number::S32) -> std::generator::GenOnce[&std::string_view::StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> std::void::Void {
        let x = a[0]
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_mut, R"(
    cls A { }

    sup A ext std::ops::idx::IndexMut[std::string_view::StrView, std::number::S32] {
        cor index_mut(&mut self, index: std::number::S32) -> std::generator::GenOnce[&mut std::string_view::StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> std::void::Void {
        let x = a[mut 0]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_mut_on_ref_only,
    SppIdentifierUnknownError, R"(
    cls A { }

    sup A ext std::ops::idx::IndexRef[std::string_view::StrView, std::number::S32] {
        cor index_ref(&self, index: std::number::S32) -> std::generator::GenOnce[&std::string_view::StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> std::void::Void {
        let x = a[mut 0]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_ref_on_mut_only,
    SppIdentifierUnknownError, R"(
    cls A { }

    sup A ext std::ops::idx::IndexMut[std::string_view::StrView, std::number::S32] {
        cor index_mut(&mut self, index: std::number::S32) -> std::generator::GenOnce[&mut std::string_view::StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> std::void::Void {
        let x = a[0]
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_ref_infer_type, R"(
    cls A { }

    sup A ext std::ops::idx::IndexRef[std::string_view::StrView, std::number::S32] {
        cor index_ref(&self, index: std::number::S32) -> std::generator::GenOnce[&std::string_view::StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> std::void::Void {
        let x: &std::string_view::StrView = a[0]
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_mut_infer_type, R"(
    cls A { }

    sup A ext std::ops::idx::IndexMut[std::string_view::StrView, std::number::S32] {
        cor index_mut(&mut self, index: std::number::S32) -> std::generator::GenOnce[&mut std::string_view::StrView] {
            gen &mut "1"
        }
    }

    fun f(mut a: A) -> std::void::Void {
        let x: &mut std::string_view::StrView = a[mut 0]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_mut_on_immutable_receiver,
    SppInvalidMutationError, R"(
    cls A { }

    sup A ext std::ops::idx::IndexMut[std::string_view::StrView, std::number::S32] {
        cor index_mut(&mut self, index: std::number::S32) -> std::generator::GenOnce[&mut std::string_view::StrView] {
            gen &mut "1"
        }
    }

    fun f(a: A) -> std::void::Void {
        let x = a[mut 0]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_argument_type_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    sup A ext std::ops::idx::IndexRef[std::string_view::StrView, std::number::S32] {
        cor index_ref(&self, index: std::number::S32) -> std::generator::GenOnce[&std::string_view::StrView] {
            gen &"1"
        }
    }

    fun f(a: A) -> std::void::Void {
        let x = a["hello"]
    }
)");
