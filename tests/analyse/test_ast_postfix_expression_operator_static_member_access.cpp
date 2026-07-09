#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_field_on_type, R"(
    cls Point {
        x: S32
        y: S32
    }

    sup Point {
        fun f() -> Void { }
    }

    fun f() -> Void {
        Point::f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_field_on_type,
    SppIdentifierUnknownError, R"(
    cls Point {
        x: S32
        y: S32
    }

    fun f(p: Point) -> Void {
        Point::f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_field_on_namespace, R"(
    fun f() -> Void {
        std::io::print("hello")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_field_on_namespace,
    SppIdentifierUnknownError, R"(
    fun f() -> Void {
        std::io::ppp(&"hello")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_static_operator_application_on_variable,
    SppMemberAccessRuntimeOperatorExpectedError, R"(
    cls Point {
        x: S32
        y: S32
    }

    fun f(p: Point) -> Void {
        let x = p::x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_private_static_member_on_type_diff_ctx,
    SppAccessViolationError, R"(
    cls MyType { }
    sup MyType {
        !private cmp n: S32 = 123
    }

    fun f() -> Void {
        let x = MyType::n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_private_static_member_on_type_same_ctx, R"(
    cls MyType { }
    sup MyType {
        !private cmp n: S32 = 123

        fun f() -> Void {
            let x = MyType::n
        }
    }
)");
