#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_required,
    R"(
    fun f[T]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_optional,
    R"(
    fun f[T = Bool]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_variadic,
    R"(
    fun f[..T: Bool]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterTypeAst,
    test_valid_optional_generic_default,
    R"(
    fun f[T = Vec[Bool]]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterTypeAst,
    test_invalid_constraint_with_convention,
    SppSecondClassBorrowViolationError, R"(
    fun f[T: &Copy]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterTypeAst,
    test_invalid_optional_unknown_default,
    SppIdentifierUnknownError, R"(
    fun f[T = Unknown]() -> Void { }
)");
