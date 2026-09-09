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
        std::console::print("hello")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_field_on_namespace,
    SppIdentifierUnknownError, R"(
    fun f() -> Void {
        std::console::ppp(&"hello")
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

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_attribute_and_constant_of_one_name_coexist, R"(
    cls Point {
        !public x: U32
    }

    sup Point {
        !public cmp x: Bool = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_constant_read_through_the_type, R"(
    cls Point { }

    sup Point {
        !public cmp x: Bool = true
    }

    fun f() -> Void {
        let a: Bool = Point::x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_constant_read_through_the_type_over_an_attribute_of_one_name, R"(
    cls Point {
        !public x: U32
    }

    sup Point {
        !public cmp x: Bool = true
    }

    fun f() -> Void {
        let a: Bool = Point::x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_attribute_read_through_the_type,
    SppMemberAccessRuntimeOperatorExpectedError, R"(
    cls Point {
        !public x: U32
    }

    fun f() -> Void {
        let a = Point::x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_constant_on_a_base_and_attribute_on_the_derived_type, R"(
    cls Base { }

    sup Base {
        !public cmp x: Bool = true
    }

    cls Point {
        !public x: U32
    }

    sup Point ext Base { }

    fun f() -> Void {
        let p = Point(x=1_u32)
        let a: U32 = p.x
        let b: Bool = Point::x
        drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_valid_attribute_on_a_base_and_constant_on_the_derived_type, R"(
    cls Base {
        !public x: U32
    }

    cls Point { }

    sup Point ext Base { }

    sup Point {
        !public cmp x: Bool = true
    }

    fun f() -> Void {
        let p = Point(x=1_u32)
        let a: U32 = p.x
        let b: Bool = Point::x
        drop(p)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorStaticMemberAccessAst,
    test_invalid_two_inherited_constants_of_one_name,
    SppAmbiguousMemberAccessError, R"(
    cls B { }

    sup B {
        !public cmp x: Bool = true
    }

    cls C { }

    sup C {
        !public cmp x: Bool = false
    }

    cls A { }

    sup A ext B { }
    sup A ext C { }

    fun f() -> Void {
        let a = A::x
    }
)");
