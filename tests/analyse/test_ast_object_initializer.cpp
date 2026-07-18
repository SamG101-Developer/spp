#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_generic_type_invalid_usage,
    SppObjectInitializerGenericWithArgsError, R"(
    fun f[T]() -> Void {
        let foo = T(a=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_generic_type_valid_usage, R"(
    fun f[T]() -> Void {
        let foo = T()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_object_initializer_generic_inference, R"(
    cls Foo[T] {
        !public a: T
    }

    fun f() -> Void {
        let foo = Foo(a=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_object_initializer_overridden_abstract_base_class, R"(
    cls Foo {
        !public a: S32
    }

    cls Bar { }

    sup Foo {
        !abstract_method
        !public
        fun f() -> Void { }
    }

    sup Bar ext Foo {
        fun f() -> Void { }
    }

    fun f() -> Void {
        let foo = Bar()
    }
)");
