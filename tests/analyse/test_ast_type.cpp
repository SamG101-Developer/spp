#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_unknown_type,
    SppIdentifierUnknownError, R"(
    fun f() -> Unknown { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_unknown_namespaced_type,
    SppIdentifierUnknownError, R"(
    fun f() -> std::Unknown { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_type_unknown_namespace,
    SppIdentifierUnknownError, R"(
    fun f() -> test::Type { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_type_unknown_namespace_nested,
    SppIdentifierUnknownError, R"(
    fun f() -> std::other::Unknown { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_type_unknown_nested_type,
    SppIdentifierUnknownError, R"(
    fun f() -> Void {
        let x: StrView::Type
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_type_generic_nested_type,
    SppIdentifierUnknownError, R"(
    fun f[T]() -> Void {
        let x: T::Type
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type, R"(
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_namespaced, R"(
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant, R"(
    fun f(mut a: Str or Bool) -> Void { a = Str::from("hello") }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_default, R"(
    fun f(a: Str or Bool = Str::from("hello")) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_tuple_1, R"(
    fun f(mut a: (Str,)) -> Void { a = (Str::from("hello"),) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_tuple_n, R"(
    fun f(mut a: (Str, Bool)) -> Void { a = (Str::from("hello"), true) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_tuple_default, R"(
    fun f(a: (Str, Bool) = (Str::from("hello"), true)) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_array, R"(
    fun f(mut a: [Bool; 3_uz]) -> Void { a = [false, false, false] }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_array_default, R"(
    fun f(a: [false; 3_uz] = [false, false, false]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_function_type_with_function_call_1, R"(
    fun f(a: FunRef[(&StrView, &StrView), Bool]) -> Void {
        let mut x = a("hello", "world")
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_function_type_with_function_call_2, R"(
    fun f(a: FunRef[(&StrView, &StrView), Bool], b: &StrView, c: &StrView) -> Void {
        let mut x = a(b, c)
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type, R"(
    cls MyType { }
    sup MyType {
        !public
        type X = Str
    }

    fun f() -> Void {
        let x: MyType::X
        x = Str::from("hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_multiple_nested_type, R"(
    cls MyTypeA { }
    cls MyTypeB { }
    cls MyTypeC { }

    sup MyTypeA {
        !public
        type X = MyTypeB
    }

    sup MyTypeB {
        !public
        type Y = MyTypeC
    }

    fun f() -> Void {
        let x: MyTypeA::X::Y
        x = MyTypeC()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type_generic, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        !public
        type X = T
    }

    fun f() -> Void {
        let x: MyType[S32]::X
        x = 10
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type_nested_generics, R"(
    cls TypeA[T, A, B] { }
    cls TypeB[U, B] { }
    cls TypeC[V] { }

    sup [V] TypeC[V] {
        !public
        type InnerC[P] = TypeB[V, P]
    }

    sup [U, B] TypeB[U, B] {
        !public
        type InnerB[Q] = TypeA[U, Q, B]
    }

    sup [T, A, B] TypeA[T, A, B] {
        !public
        type InnerA[R] = (T, B, A, R)
    }

    fun f() -> Void {
        let x: TypeC[S32]::InnerC[StrView]::InnerB[Bool]::InnerA[U64]
        x = (10, "hello", false, 10_u64)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type_at_top_level, R"(
    cls TypeA { }
    sup TypeA {
        !public
        type X = StrView
    }

    fun f(a: TypeA::X) -> TypeA::X {
        ret "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_inherited_nested_type, R"(
    cls Base { }
    sup Base {
        !public
        type X = StrView
    }

    cls C { }
    sup C ext Base { }

    fun f() -> Void {
        let x: C::X
        x = "hello"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_ambiguous_nested_type,
    SppAmbiguousMemberAccessError, R"(
    cls Base1 { }
    sup Base1 {
        !public
        type X = StrView
    }

    cls Base2 { }
    sup Base2 {
        !public
        type X = Bool
    }

    cls C { }
    sup C ext Base1 { }
    sup C ext Base2 { }

    fun f() -> Void {
        let x: C::X
    }
)");
