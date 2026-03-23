#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementAst,
    test_invalid_expression_type,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        let x = std::boolean::Bool
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementAst,
    test_invalid_variant_destructure,
    SppTypeMismatchError, R"(
    cls A { a: std::boolean::Bool }
    cls B { b: std::boolean::Bool }

    fun f(value: A or B) -> std::void::Void {
        let A(a) = value
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementAst,
    test_invalid_explicit_type_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let x: std::boolean::Bool = 123
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementAst,
    test_invalid_explicit_type_mismatch_conv_ref_vs_mov,
    SppTypeMismatchError, R"(
    fun f(b: &std::boolean::Bool) -> std::void::Void {
        let x: std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementAst,
    test_invalid_explicit_type_mismatch_conv_mov_vs_ref,
    SppTypeMismatchError, R"(
    fun f(b: std::boolean::Bool) -> std::void::Void {
        let x: &std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementAst,
    test_invalid_explicit_type_hint_4, // todo ?
    SppInvalidTypeAnnotationError, R"(
    cls A { a: std::boolean::Bool }

    fun f(aa: A) -> std::void::Void {
        let A(a): A = aa
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_initialized, R"(
    fun f() -> std::void::Void {
        let x = true
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_uninitialized, R"(
    fun f() -> std::void::Void {
        let x: std::boolean::Bool
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_conv_mov, R"(
    fun f(b: std::boolean::Bool) -> std::void::Void {
        let mut x: std::boolean::Bool
        x = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_conv_mut, R"(
    fun f(b: &mut std::boolean::Bool) -> std::void::Void {
        let mut x: &mut std::boolean::Bool
        x = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_conv_ref, R"(
    fun f(b: &std::boolean::Bool) -> std::void::Void {
        let mut x: &std::boolean::Bool
        x = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_explicit_type_conv_mov, R"(
    fun f(b: std::boolean::Bool) -> std::void::Void {
        let x: std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_explicit_type_conv_mut, R"(
    fun f(b: &mut std::boolean::Bool) -> std::void::Void {
        let x: &mut std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_explicit_type_conv_ref, R"(
    fun f(b: &std::boolean::Bool) -> std::void::Void {
        let x: &std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_variant_2_types, R"(
    fun f() -> std::void::Void {
        let x: std::string_view::StrView or std::number::S32 = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_variant_3_Types, R"(
    fun f() -> std::void::Void {
        let x: std::string::Str or std::number::S32 or std::boolean::Bool = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementAst,
    test_valid_variant_with_generic, R"(
    fun f() -> std::void::Void {
        let x: std::option::Opt[std::number::S32] = std::option::Some(val=123)
    }
)");
