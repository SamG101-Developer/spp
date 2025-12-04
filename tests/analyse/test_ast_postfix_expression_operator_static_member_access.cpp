#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_field_on_type, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    sup Point {
        fun f() -> std::void::Void { }
    }

    fun f() -> std::void::Void {
        Point::f()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_field_on_type,
    SppIdentifierUnknownError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f(p: Point) -> std::void::Void {
        Point::f()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_field_on_namespace, R"(
    fun f() -> std::void::Void {
        std::io::print(&"hello")
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_field_on_namespace,
    SppIdentifierUnknownError, R"(
    fun f() -> std::void::Void {
        std::io::ppp(&"hello")
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_static_operator_application_on_variable,
    SppMemberAccessRuntimeOperatorExpectedError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f(p: Point) -> std::void::Void {
        let x = p::x
    }
)");
