#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_required_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    fun f[cmp n: &mut Bool]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_required_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    fun f[cmp n: &Bool]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_optional_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    fun f[cmp n: &mut Bool = false]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_optional_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    fun f[cmp n: &Bool = false]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_variadic_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    fun f[cmp ..n: &Bool]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_variadic_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    fun f[cmp ..n: &mut Bool]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterCompAst,
    test_valid_required,
    R"(
    fun f[cmp n: Bool]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterCompAst,
    test_valid_optional,
    R"(
    fun f[cmp n: Bool = false]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterCompAst,
    test_valid_variadic,
    R"(
    fun f[cmp ..n: Bool]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_optional_default_type_mismatch,
    SppTypeMismatchError, R"(
    fun f[cmp n: USize = false]() -> Void { }
)");
