#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_expression_type,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        let x = Bool
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_variant_destructure,
    SppTypeMismatchError, R"(
    cls A { a: Bool }
    cls B { b: Bool }

    fun f(value: A or B) -> Void {
        let A(a) = value
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_explicit_type_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x: Bool = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_explicit_type_mismatch_conv_ref_vs_mov,
    SppTypeMismatchError, R"(
    fun f(b: &Bool) -> Void {
        let x: Bool = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_explicit_type_mismatch_conv_mov_vs_ref,
    SppTypeMismatchError, R"(
    fun f(b: Bool) -> Void {
        let x: &Bool = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_explicit_type_on_object_destructure,
    SppInvalidLocalVariableTypeAnnotationError, R"(
    cls A { a: Bool }

    fun f(aa: A) -> Void {
        let A(a): A = aa
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementInitializedAst,
    test_invalid_explicit_type_on_tuple_destructure,
    SppInvalidLocalVariableTypeAnnotationError, R"(
    fun f() -> Void {
        let (a, b): (Bool, Bool) = (true, false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_initialized, R"(
    fun f() -> Void {
        let x = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_inferred_type, R"(
    fun f() -> Void {
        let x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_explicit_type_matches_inferred, R"(
    fun f() -> Void {
        let x: S32 = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_explicit_type_conv_mov, R"(
    fun f(b: Bool) -> Void {
        let x: Bool = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_explicit_type_conv_mut, R"(
    fun f(b: &mut Bool) -> Void {
        let x: &mut Bool = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_explicit_type_conv_ref, R"(
    fun f(b: &Bool) -> Void {
        let x: &Bool = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_variant_2_types, R"(
    fun f() -> Void {
        let x: StrView or S32 = "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_variant_3_types, R"(
    fun f() -> Void {
        let x: Str or S32 or Bool = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementInitializedAst,
    test_valid_variant_with_generic, R"(
    fun f() -> Void {
        let x: Opt[S32] = Some(val=123)
    }
)");
