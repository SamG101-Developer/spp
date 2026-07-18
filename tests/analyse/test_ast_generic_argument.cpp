#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_named_expression, R"(
    fun f[cmp n: Bool]() -> Void { }

    fun g() -> Void {
        f[n=true]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_unnamed_expression, R"(
    fun f[cmp n: Bool]() -> Void { }

    fun g() -> Void {
        f[true]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_named_type, R"(
    fun f[T]() -> Void { }

    fun g() -> Void {
        f[T=Bool]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_unnamed_type, R"(
    fun f[T]() -> Void { }

    fun g() -> Void {
        f[Bool]()
    }
)");
