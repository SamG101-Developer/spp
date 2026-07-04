#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_instead_of_type_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[T] { }

    fun g[T]() -> A[T] { ret A[T]() }

    fun f() -> Void {
        g[123]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_instead_of_type_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[..T] { }

    fun g[..T]() -> A[T] { ret A[T]() }

    fun f() -> Void {
        g[123, 456, 789]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_mix_instead_of_type_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[..T] { }

    fun g[..T]() -> A[T] { ret A[T]() }

    fun f() -> Void {
        g[Str, 123, 456, 789]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_generic_properly, R"(
    cls A[T] { }

    fun g[T]() -> A[T] { ret A[T]() }

    fun f() -> Void {
        g[Str]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_generic_variadic_properly, R"(
    cls A[..T] { }

    fun g[..T]() -> A[T] { ret A[T]() }

    fun f() -> Void {
        g[Str, U32, U64]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_instead_of_comp_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp n: Bool] { }

    fun g[cmp n: Bool]() -> A[n] { ret A[n]() }

    fun f() -> Void {
        g[Str]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_type_instead_of_comp_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp ..n: Bool] { }

    fun g[cmp ..n: Bool]() -> A[n] { ret A[n]() }

    fun f() -> Void {
        g[Str, U32, U64]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_mix_instead_of_comp_generic_variadic,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp ..n: Bool] { }

    fun g[cmp ..n: Bool]() -> A[n] { ret A[n]() }

    fun f() -> Void {
        g[123, Str, U32, U64]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_generic_properly, R"(
    cls A[cmp n: Bool] { }

    fun g[cmp n: Bool]() -> A[n] { ret A[n]() }

    fun f() -> Void {
        g[false]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_use_comp_generic_variadic_properly, R"(
    cls A[cmp ..n: Bool] { }

    fun g[cmp ..n: Bool]() -> A[n] { ret A[n]() }

    fun f() -> Void {
        g[false, true, false]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_type_match_same_type_same_generic_order, R"(
    cls Type[T, U] { }

    fun f() -> Void {
        let mut type1 = Type[Bool, Str]()
        type1 = Type[T=Bool, U=Str]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_type_match_same_type_different_generic_order, R"(
    cls Type[T, U] { }

    fun f() -> Void {
        let mut type1 = Type[Bool, Str]()
        type1 = Type[U=Str, T=Bool]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_generic_replaced_with_generic, R"(
    fun g[U]() -> Vec[U] {
        ret Vec[U]()
    }

    fun f() -> Void {
        let mut x = g[StrView]()
        x.push_back(element="hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericArgumentGroup,
    test_generic_replaced_with_generic_stateful, R"(
    fun g[U]() -> Opt[U] {
        ret Some[U]()
    }

    fun f() -> Void {
        let mut x = g[StrView]()
        x = Some(val="hello")
    }
)");
