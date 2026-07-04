#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_unnamed_argument_1,
    SppObjectInitializerInvalidArgumentError, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(123)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_unnamed_argument_2,
    SppObjectInitializerInvalidArgumentError, R"(
    cls Foo[T] {
        !public a: T
    }

    fun f() -> std::void::Void {
        let foo = Foo(123)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_named_argument_expression_type,
    SppInvalidPrimaryExpressionError, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=std::number::S32)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_named_argument_type_mismatch,
    SppTypeMismatchError, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=true)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_invalid_argument_name,
    SppArgumentNameInvalidError, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(b=1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_duplicate_argument,
    SppIdentifierDuplicateError, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=1, a=2)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_multiple_autofill_arguments,
    SppObjectInitializerMultipleAutofillArgumentsError, R"(
    cls Foo {
        !public a: std::number::S32
        !public b: std::number::S32
    }

    fun f(x: Foo, y: Foo) -> std::void::Void {
        let foo = Foo(..x, ..y)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_autofill_type_mismatch,
    SppTypeMismatchError, R"(
    cls Foo {
        !public a: std::number::S32
        !public b: std::number::S32
    }

    cls Bar {
        !public a: std::number::S32
    }

    fun f(bar: Bar) -> std::void::Void {
        let foo = Foo(..bar)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_valid_object_initializer_named_argument, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_valid_object_initializer_shorthand_argument, R"(
    cls Foo {
        !public a: std::number::S32
    }

    fun f() -> std::void::Void {
        let a = 1
        let foo = Foo(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_valid_object_initializer_autofill_argument, R"(
    cls Foo {
        !public a: std::number::S32
        !public b: std::number::S32
    }

    fun f(other: Foo) -> std::void::Void {
        let foo = Foo(a=1, ..other)
    }
)");
