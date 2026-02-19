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
    fun f() -> std::void::Void {
        let x: std::string_view::StrView::Type
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeAst,
    test_invalid_type_generic_nested_type,
    SppGenericTypeInvalidUsageError, R"(
    fun f[T]() -> std::void::Void {
        let x: T::Type
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type, R"(
    fun f() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_namespaced, R"(
    fun f() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant, R"(
    fun f(mut a: std::string_view::StrView or std::boolean::Bool) -> std::void::Void { a = "hello" }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_default, R"(
    fun f(a: std::string_view::StrView or std::boolean::Bool = "hello") -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_tuple_1, R"(
    fun f(mut a: (std::string_view::StrView,)) -> std::void::Void { a = ("hello",) }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_tuple_n, R"(
    fun f(mut a: (std::string_view::StrView, std::boolean::Bool)) -> std::void::Void { a = ("hello", true) }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_shorthand_variant_tuple_default, R"(
    fun f(a: (std::string_view::StrView, std::boolean::Bool) = ("hello", true)) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_function_type_with_function_call_1, R"(
    fun f(a: std::function::FunRef[(std::string_view::StrView, std::string_view::StrView), std::boolean::Bool]) -> std::void::Void {
        let mut x = a("hello", "world")
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_type_function_type_with_function_call_2, R"(
    fun f(a: std::function::FunRef[(&std::string_view::StrView, &std::string_view::StrView), std::boolean::Bool], b: &std::string_view::StrView, c: &std::string_view::StrView) -> std::void::Void {
        let mut x = a(b, c)
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type, R"(
    cls MyType { }
    sup MyType {
        type X = std::string_view::StrView
    }

    fun f() -> std::void::Void {
        let x: MyType::X
        x = "hello"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_multiple_nested_type, R"(
    cls MyTypeA { }
    cls MyTypeB { }
    cls MyTypeC { }

    sup MyTypeA {
        type X = MyTypeB
    }

    sup MyTypeB {
        type Y = MyTypeC
    }

    fun f() -> std::void::Void {
        let x: MyTypeA::X::Y
        x = MyTypeC()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type_generic, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        type X = T
    }

    fun f() -> std::void::Void {
        let x: MyType[std::number::S32]::X
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
        type InnerC[P] = TypeB[V, P]
    }

    sup [U, B] TypeB[U, B] {
        type InnerB[Q] = TypeA[U, Q, B]
    }

    sup [T, A, B] TypeA[T, A, B] {
        type InnerA[R] = (T, B, A, R)
    }

    fun f() -> std::void::Void {
        let x: TypeC[std::number::S32]::InnerC[std::string_view::StrView]::InnerB[std::boolean::Bool]::InnerA[std::number::U64]
        x = (10, "hello", false, 10_u64)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeAst,
    test_valid_nested_type_at_top_level, R"(
    cls TypeA { }
    sup TypeA {
        type X = std::string_view::StrView
    }

    fun f(a: TypeA::X) -> TypeA::X {
        ret "hello world"
    }
)");
