#include "test_macros.hpp"

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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  GenericParameterCompGenericClass,
  test_valid_comp_parameter_typed_by_the_class_generic, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun g[cmp p: Box[T]](&self) -> Void { }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.g[Box(v=2)]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericParameterCompAst,
    test_invalid_unused_default_dividing_by_zero,
    SppDivisionByZeroError, R"(
    cls B[cmp n: USize = 7_uz / 0_uz] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericParameterCompAst,
    test_valid_unused_default_reading_a_constant_written_after_it, R"(
    cls B[cmp n: USize = k * 2_uz] { }
    cmp k: USize = 1_uz + 2_uz
)");
