#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_instead_of_type_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[T] { }

    fun g[T]() -> A[T] { ret A[T]() }

    fun f() -> std::void::Void {
        g[123]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_instead_of_type_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[..T] { }

    fun g[..T]() -> A[T] { ret A[T]() }

    fun f() -> std::void::Void {
        g[123, 456, 789]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_mix_instead_of_type_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[..T] { }

    fun g[..T]() -> A[T] { ret A[T]() }

    fun f() -> std::void::Void {
        g[std::string::Str, 123, 456, 789]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_generic_properly, R"(
    cls A[T] { }

    fun g[T]() -> A[T] { ret A[T]() }

    fun f() -> std::void::Void {
        g[std::string::Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_generic_variadic_properly, R"(
    cls A[..T] { }

    fun g[..T]() -> A[T] { ret A[T]() }

    fun f() -> std::void::Void {
        g[std::string::Str, std::number::U32, std::number::U64]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_instead_of_comp_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp n: std::boolean::Bool] { }

    fun g[cmp n: std::boolean::Bool]() -> A[n] { ret A[n]() }

    fun f() -> std::void::Void {
        g[std::string::Str]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_instead_of_comp_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp ..n: std::boolean::Bool] { }

    fun g[cmp ..n: std::boolean::Bool]() -> A[n] { ret A[n]() }

    fun f() -> std::void::Void {
        g[std::string::Str, std::number::U32, std::number::U64]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_mix_instead_of_comp_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp ..n: std::boolean::Bool] { }

    fun g[cmp ..n: std::boolean::Bool]() -> A[n] { ret A[n]() }

    fun f() -> std::void::Void {
        g[123, std::string::Str, std::number::U32, std::number::U64]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_generic_properly, R"(
    cls A[cmp n: std::boolean::Bool] { }

    fun g[cmp n: std::boolean::Bool]() -> A[n] { ret A[n]() }

    fun f() -> std::void::Void {
        g[false]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_generic_variadic_properly, R"(
    cls A[cmp ..n: std::boolean::Bool] { }

    fun g[cmp ..n: std::boolean::Bool]() -> A[n] { ret A[n]() }

    fun f() -> std::void::Void {
        g[false, true, false]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_type_match_same_type_same_generic_order, R"(
    cls Type[T, U] { }

    fun f() -> std::void::Void {
        let mut type1 = Type[std::boolean::Bool, std::string::Str]()
        type1 = Type[T=std::boolean::Bool, U=std::string::Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_type_match_same_type_different_generic_order, R"(
    cls Type[T, U] { }

    fun f() -> std::void::Void {
        let mut type1 = Type[std::boolean::Bool, std::string::Str]()
        type1 = Type[U=std::string::Str, T=std::boolean::Bool]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_generic_replaced_with_generic, R"(
    use std::string::Str
    use std::vector::Vec

    fun g[U]() -> Vec[U] {
        ret Vec[U]()
    }

    fun f() -> std::void::Void {
        let mut x = g[std::string::Str]()
        x.push(element="hello")
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_generic_replaced_with_generic_stateful, R"(
    use std::option::Opt
    use std::option::Some
    use std::string::Str

    fun g[U]() -> Opt[U] {
        ret Some[U]()
    }

    fun f() -> std::void::Void {
        let mut x = g[std::string::Str]()
        x = Some(val="hello")
    }
)");
