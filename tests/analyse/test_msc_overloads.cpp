#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_parameter_conventions_1,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_parameter_conventions_2,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
    fun f(a: &std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_different_return_type, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool) -> std::boolean::Bool { ret true }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_count, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_1, R"(
    fun f(a: &std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_2, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: &std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_3, R"(
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_conventions_4, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_parameter_types, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::bignum::bigint::BigInt) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_generics_same_name,
    SppFunctionPrototypeConflictError, R"(
    fun f[T](a: T) -> std::void::Void { }
    fun f[T](a: T) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_invalid_overload_generics_different_name,
    SppFunctionPrototypeConflictError, R"(
    fun f[T](a: T) -> std::void::Void { }
    fun f[U](a: U) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_generics_usage_1, R"(
    fun f[T]() -> std::void::Void { }
    fun f[T](b: T) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_generics_usage_2, R"(
    fun f[T](a: T) -> std::void::Void { }
    fun f[T](a: T, b: T) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_FreeFuncs,
    test_valid_overload_generics_usage_3, R"(
    fun f[T]() -> std::void::Void { }
    fun f[T, U]() -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_parameter_conventions_1,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(a: &std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_parameter_conventions_2,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: &std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_different_return_type, R"(
    cls A { }
    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: std::boolean::Bool) -> std::boolean::Bool { ret true }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_count, R"(
    cls A { }
    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_1, R"(
    cls A { }
    sup A {
        fun f(a: &std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_2, R"(
    cls A { }
    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: &std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_3, R"(
    cls A { }
    sup A {
        fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_conventions_4, R"(
    cls A { }
    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: &mut std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_parameter_types, R"(
    cls A { }
    sup A {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }

    sup A {
        fun f(a: std::bignum::bigint::BigInt) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_generics_same_name,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> std::void::Void { }
    }

    sup A {
        fun f[T](a: T) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverloads_SupBlocks,
    test_invalid_overload_generics_different_name,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> std::void::Void { }
    }

    sup A {
        fun f[U](a: U) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_generics_usage_1, R"(
    cls A { }
    sup A {
        fun f[T]() -> std::void::Void { }
    }

    sup A {
        fun f[T](b: T) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_generics_usage_2, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> std::void::Void { }
    }

    sup A {
        fun f[T](a: T, b: T) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverloads_SupBlocks,
    test_valid_overload_generics_usage_3, R"(
    cls A { }
    sup A {
        fun f[T]() -> std::void::Void { }
    }

    sup A {
        fun f[T, U]() -> std::void::Void { }
    }
)");
