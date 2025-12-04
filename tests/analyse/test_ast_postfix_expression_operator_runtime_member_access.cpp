#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_indexing_non_indexable_type,
    SppMemberAccessNonIndexableError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f(p: Point) -> std::void::Void {
        p.0
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_tuple_index_out_of_bounds,
    SppMemberAccessOutOfBoundsError, R"(
    fun f(p: (std::number::S32, std::number::S32)) -> std::void::Void {
        p.2
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_tuple_index_type_mismatch,
    SppTypeMismatchError, R"(
    fun f(p: (std::number::S32, std::string::Str)) -> std::void::Void {
        let mut x = p.0
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_tuple_index, R"(
    fun f(p: (std::number::S32, std::number::S32)) -> std::void::Void {
        p.0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_tuple_index_type_match, R"(
    fun f(p: (std::number::S32, std::string::Str)) -> std::void::Void {
        let mut x = p.0
        x = 123
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_array_index_out_of_bounds,
    SppMemberAccessOutOfBoundsError, R"(
    fun f(p: [std::boolean::Bool; 2_uz]) -> std::void::Void {
        p.2
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_array_index_type_mismatch,
    SppTypeMismatchError, R"(
    fun f(p: [std::boolean::Bool; 2_uz]) -> std::void::Void {
        let mut x = p.0
        x = 123
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_array_index, R"(
    fun f(p: [std::string::Str; 2_uz]) -> std::void::Void {
        let mut x = p.0
        x = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_array_index_type_check, R"(
    fun f(p: [std::string::Str; 2_uz]) -> std::void::Void {
        let mut x = p.0
        x = "hello world"
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_generic_type_indexing,
    SppGenericTypeInvalidUsageError, R"(
    fun f[T](p: T) -> std::void::Void {
        p.0
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_generic_type_member_access,
    SppGenericTypeInvalidUsageError, R"(
    fun f[T](p: T) -> std::void::Void {
        p.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_field_on_variable,
    SppIdentifierUnknownError, R"(
    fun f(p: std::number::S32) -> std::void::Void {
        p.x
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_valid_field_on_variable, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f(p: Point) -> std::void::Void {
        p.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_runtime_operator_application_on_namespace,
    SppMemberAccessStaticOperatorExpectedError, R"(
    fun f() -> std::void::Void {
        std.print()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorRuntimeMemberAccessAst,
    test_invalid_runtime_operator_application_on_type,
    SppMemberAccessStaticOperatorExpectedError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    sup Point {
        fun f() -> std::void::Void { }
    }

    fun f() -> std::void::Void {
        Point.f()
    }
)");
