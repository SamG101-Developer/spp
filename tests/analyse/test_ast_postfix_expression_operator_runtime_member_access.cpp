#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_indexing_non_indexable_type,
    SppMemberAccessNonIndexableError, R"(
    cls Point {
        x: S32
        y: S32
    }

    fun f(p: Point) -> Void {
        p.0
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_tuple_index_out_of_bounds,
    SppMemberAccessOutOfBoundsError, R"(
    fun f(p: (S32, S32)) -> Void {
        p.2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_tuple_index_type_mismatch,
    SppTypeMismatchError, R"(
    fun f(p: (S32, Str)) -> Void {
        let mut x = p.0
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_tuple_index, R"(
    fun f(p: (S32, S32)) -> Void {
        p.0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_tuple_index_type_match, R"(
    fun f(p: (S32, Str)) -> Void {
        let mut x = p.0
        x = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_array_index_out_of_bounds,
    SppMemberAccessOutOfBoundsError, R"(
    fun f(p: [Bool; 2_uz]) -> Void {
        p.2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_array_index_type_mismatch,
    SppTypeMismatchError, R"(
    fun f(p: [Bool; 2_uz]) -> Void {
        let mut x = p.0
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_array_index, R"(
    fun f(p: [StrView; 2_uz]) -> Void {
        let mut x = p.0
        x = "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_array_index_type_check, R"(
    fun f(p: [StrView; 2_uz]) -> Void {
        let mut x = p.0
        x = "hello world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_generic_type_indexing,
    SppMemberAccessNonIndexableError, R"(
    fun f[T](p: T) -> Void {
        p.0
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_generic_type_member_access,
    SppIdentifierUnknownError, R"(
    fun f[T](p: T) -> Void {
        p.x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_field_on_variable,
    SppIdentifierUnknownError, R"(
    fun f(p: S32) -> Void {
        p.x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_field_on_variable, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> Void {
        p.x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_field_type_match, R"(
    cls Point {
        !public x: Str
        !public y: S32
    }

    fun f(p: Point) -> Void {
        let mut x = p.x
        x = "hello world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_field_type_mismatch,
    SppTypeMismatchError, R"(
    cls Point {
        !public x: Str
        !public y: S32
    }

    fun f(p: Point) -> Void {
        let mut x = p.x
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_field_copy_on_borrowed_variable, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: &Point) -> Void {
        let y = p.x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_nested_field_access, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    cls Line {
        !public start: Point
        !public end: Point
    }

    fun f(l: Line) -> Void {
        let mut x = l.start.x
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_forwarded_member_access, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: std::shared::Shared[Point]) -> Void {
        p.x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_runtime_operator_application_on_namespace,
    SppMemberAccessStaticOperatorExpectedError, R"(
    fun f() -> Void {
        std.print()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_runtime_operator_application_on_type,
    SppMemberAccessStaticOperatorExpectedError, R"(
    cls Point {
        x: S32
        y: S32
    }

    sup Point {
        fun f() -> Void { }
    }

    fun f() -> Void {
        Point.f()
    }
)");
