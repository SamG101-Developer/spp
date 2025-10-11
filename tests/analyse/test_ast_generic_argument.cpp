#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_named_expression, R"(
    fun f[cmp n: std::boolean::Bool]() -> std::void::Void { }

    fun g() -> std::void::Void {
        f[n=true]()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_unnamed_expression, R"(
    fun f[cmp n: std::boolean::Bool]() -> std::void::Void { }

    fun g() -> std::void::Void {
        f[true]()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_named_type, R"(
    fun f[T]() -> std::void::Void { }

    fun g() -> std::void::Void {
        f[T=std::boolean::Bool]()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentAst,
    test_valid_unnamed_type, R"(
    fun f[T]() -> std::void::Void { }

    fun g() -> std::void::Void {
        f[std::boolean::Bool]()
    }
)")
