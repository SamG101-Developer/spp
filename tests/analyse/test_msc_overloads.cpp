#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_parameter_conventions_1,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: &Bool) -> Void { }
    fun f(a: &mut Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_parameter_conventions_2,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: &mut Bool) -> Void { }
    fun f(a: &Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_different_return_type, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool) -> Bool { ret true }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_count, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool, b: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_1, R"(
    fun f(a: &Bool) -> Void { }
    fun f(a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_2, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: &Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_3, R"(
    fun f(a: &mut Bool) -> Void { }
    fun f(a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_4, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: &mut Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_types, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: std::bignum::bigint::BigInt) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_generics_same_name,
    SppFunctionPrototypeConflictError, R"(
    fun f[T](a: T) -> Void { }
    fun f[T](a: T) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_generics_different_name,
    SppFunctionPrototypeConflictError, R"(
    fun f[T](a: T) -> Void { }
    fun f[U](a: U) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_generics_usage_1, R"(
    fun f[T]() -> Void { }
    fun f[T](b: T) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_generics_usage_2, R"(
    fun f[T](a: T) -> Void { }
    fun f[T](a: T, b: T) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_generics_usage_3, R"(
    fun f[T]() -> Void { }
    fun f[T, U]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_different_parameter_names,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: Bool) -> Void { }
    fun f(b: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_subroutine_and_coroutine, R"(
    fun f(a: Bool) -> Void { }
    cor f(a: Bool) -> Gen[Bool] { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_parameter_conventions_1,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(a: &Bool) -> Void { }
    }

    sup A {
        fun f(a: &mut Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_parameter_conventions_2,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(a: &mut Bool) -> Void { }
    }

    sup A {
        fun f(a: &Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_different_return_type, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool) -> Bool { ret true }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_count, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool, b: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_1, R"(
    cls A { }
    sup A {
        fun f(a: &Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_2, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: &Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_3, R"(
    cls A { }
    sup A {
        fun f(a: &mut Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_4, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: &mut Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_types, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: std::bignum::bigint::BigInt) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_generics_same_name,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> Void { }
    }

    sup A {
        fun f[T](a: T) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_generics_different_name,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> Void { }
    }

    sup A {
        fun f[U](a: U) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_generics_usage_1, R"(
    cls A { }
    sup A {
        fun f[T]() -> Void { }
    }

    sup A {
        fun f[T](b: T) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_generics_usage_2, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> Void { }
    }

    sup A {
        fun f[T](a: T, b: T) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_generics_usage_3, R"(
    cls A { }
    sup A {
        fun f[T]() -> Void { }
    }

    sup A {
        fun f[T, U]() -> Void { }
    }
)");

// Two methods that differ only in their `self` convention do not conflict.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_different_self_conventions, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    sup A {
        fun f(&mut self) -> Void { }
    }
)");

// A method subroutine and coroutine with the same value signature do not conflict.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_subroutine_and_coroutine, R"(
    cls A { }
    sup A {
        fun f(&self, a: Bool) -> Void { }
    }

    sup A {
        cor f(&self, a: Bool) -> Gen[Bool] { }
    }
)");
