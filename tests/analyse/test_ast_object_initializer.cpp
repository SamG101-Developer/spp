#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_generic_type_invalid_usage,
    SppObjectInitializerGenericWithArgsError, R"(
    fun f[T]() -> std::void::Void {
        let foo = T(a=1)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_generic_type_valid_usage, R"(
    fun f[T]() -> std::void::Void {
        let foo = T()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_object_initializer, R"(
    cls Foo {
        a: std::number::S32
    }

    fun f() -> std::void::Void {
        let foo = Foo(a=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_object_initializer_overridden_abstract_base_class, R"(
    cls Foo {
        a: std::number::S32
    }

    cls Bar { }

    sup Foo {
        @abstract_method
        fun f() -> std::void::Void { }
    }

    sup Bar ext Foo {
        fun f() -> std::void::Void { }
    }

    fun f() -> std::void::Void {
        let foo = Bar()
    }
)");

