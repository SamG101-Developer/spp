#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_generic_type_invalid_usage,
    SppArgumentNameInvalidError, R"(
    fun f[T]() -> Void {
        let foo = T(a=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_generic_type_valid_usage, R"(
    fun f[T]() -> Void {
        let foo = T()
        std::mem::ops::drop(foo)
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
        std::mem::ops::drop(foo)
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
        std::mem::ops::drop(foo)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_object_initializer_generic_constrained_attribute, R"(
    cls Foo {
        !public a: S32
    }

    fun f[T: Foo]() -> Void {
        let foo = T(a=1)
        std::mem::ops::drop(foo)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_object_initializer_generic_constrained_attribute_nested_generic, R"(
    cls Foo {
        !public a: S32
    }

    cls Wrapper[U] {
        !public inner: U
    }

    fun f[T: Foo]() -> Void {
        let foo = Wrapper[T](inner=T(a=1))
        std::mem::ops::drop(foo)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_object_initializer_generic_constrained_attribute_default_filled, R"(
    cls Foo {
        !public a: S32
    }

    cls Wrapper[U] {
        !public inner: U
    }

    fun g[T: Foo]() -> Wrapper[T] {
        ret Wrapper[T]()
    }

    fun f() -> Void {
        let x = g[Foo]()
        std::mem::ops::drop(x)
    }
)");

// A plain alias of "Vec" links the template's scope, where the attribute "RawBuf[T, A]" has no symbol.
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_object_initializer_through_an_alias_of_a_vector, R"(
    type VecAlias = Vec[S32]

    fun f() -> Void {
        let v = VecAlias()
        std::mem::ops::drop(v)
    }
)");

// A zero type has exactly one value, written as the type's name. Writing it as an initializer ("Marker()") is an
// error wherever the type is named concretely; through a generic parameter ("T()") it is how a value of whatever the
// parameter becomes is asked for, so it is allowed whatever that turns out to be.

SPP_TEST_SHOULD_FAIL_SEMANTIC_AT(
    AstObjectInitializerAst,
    test_invalid_zero_type_initialized,
    SppObjectInitializerZeroTypeError, "Marker()", R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    fun f() -> Void {
        let m = Marker()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_invalid_none_initialized,
    SppObjectInitializerZeroTypeError, R"(
    fun f() -> Opt[S32] {
        ret None()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_invalid_zero_type_initialized_through_an_alias,
    SppObjectInitializerZeroTypeError, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    type Alias = Marker

    fun f() -> Void {
        let m = Alias()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_invalid_zero_type_initialized_as_self,
    SppObjectInitializerZeroTypeError, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    sup Marker {
        fun make() -> Self {
            ret Self()
        }
    }
)");

// Inside a generic function, a zero type named concretely is still written concretely.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstObjectInitializerAst,
    test_invalid_zero_type_initialized_concretely_in_a_generic_function,
    SppObjectInitializerZeroTypeError, R"(
    fun none_of[T]() -> Opt[T] {
        ret None()
    }

    fun f() -> Void {
        let x = none_of[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_zero_type_used_by_name, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    fun g() -> Opt[S32] {
        ret None
    }

    fun f() -> Void {
        let m = Marker
        let x = g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_generic_parameter_initialized_as_a_zero_type, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    fun make[T]() -> T {
        ret T()
    }

    fun f() -> Void {
        let m = make[Marker]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_generic_parameter_initialized_as_an_ordinary_type, R"(
    cls Empty { }

    fun make[T]() -> T {
        ret T()
    }

    fun f() -> Void {
        let e = make[Empty]()
        std::mem::ops::drop(e)
    }
)");

// A default argument is copied into each call, where the parameter has no name: it is still "A()" as written.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_generic_default_argument_as_a_zero_type, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    fun take[A](a: A = A()) -> A {
        ret a
    }

    fun f() -> Void {
        let m = take[Marker]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_method_called_on_a_zero_type_by_name, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    sup Marker {
        !public
        fun same(self) -> Self {
            ret self
        }
    }

    fun f() -> Void {
        let m = Marker.same()
    }
)");

// The compiler builds an initializer for its own analysis here; only what the author writes is held to the rule.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstObjectInitializerAst,
    test_valid_uninitialised_let_of_a_zero_type, R"(
    !zero_type
    cls Marker { }

    sup Marker ext Copy { }

    fun f() -> Void {
        let m: Marker
        m = Marker
    }
)");
