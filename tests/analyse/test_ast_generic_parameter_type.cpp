#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_required,
    R"(
    fun f[T]() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_optional,
    R"(
    fun f[T = std::boolean::Bool]() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_variadic,
    R"(
    fun f[..T: std::boolean::Bool]() -> std::void::Void { }
)");
