#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_on_non_indexable_type,
    SppExpressionNotIndexableError, R"(
    fun f() -> std::void::Void {
        123[0]
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_valid_indexing_ref, R"(
    cls A { }

    sup A ext std::iterator::IndexRef[std::string::Str, std::number::S32] {
        cor index_ref(&self, index: std::number::S32) -> std::generator::GenOnce[&std::string::Str] {
            gen &"1"
            gen &"2"
            gen &"3"
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

    sup A ext std::iterator::IndexMut[std::string::Str, std::number::S32] {
        cor index_mut(&mut self, index: std::number::S32) -> std::generator::GenOnce[&mut std::string::Str] {
            gen &mut "1"
            gen &mut "2"
            gen &mut "3"
        }
    }

    fun f(mut a: A) -> std::void::Void {
        let x = a[mut 0]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_mut_on_ref_only,
    SppExpressionNotIndexableError, R"(
    cls A { }

    sup A ext std::iterator::IndexRef[std::string::Str, std::number::S32] {
        cor index_ref(&self, index: std::number::S32) -> std::generator::GenOnce[&std::string::Str] {
            gen &"1"
            gen &"2"
            gen &"3"
        }
    }

    fun f(a: A) -> std::void::Void {
        let x = a[mut 0]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorAst,
    test_invalid_indexing_ref_on_mut_only,
    SppExpressionNotIndexableError, R"(
    cls A { }

    sup A ext std::iterator::IndexMut[std::string::Str, std::number::S32] {
        cor index_mut(&mut self, index: std::number::S32) -> std::generator::GenOnce[&mut std::string::Str] {
            gen &mut "1"
            gen &mut "2"
            gen &mut "3"
        }
    }

    fun f(mut a: A) -> std::void::Void {
        let x = a[0]
    }
)");
