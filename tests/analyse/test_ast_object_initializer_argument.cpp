#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_unnamed_argument_1,
    SppObjectInitializerInvalidArgumentError, R"(
    cls Foo {
        a: std::number::S32
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
        a: T
    }

    fun f() -> std::void::Void {
        let foo = Foo(123)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_invalid_object_initializer_named_argument_expression_type,
    SppExpressionTypeInvalidError, R"(
    cls Foo {
        a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=std::number::S32)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerArgumentAst,
    test_valid_object_initializer_named_argument, R"(
    cls Foo {
        a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=1)
    }
)");
