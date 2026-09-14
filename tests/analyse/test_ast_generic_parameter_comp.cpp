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

// Todo: red - a method's comp parameter typed by its generic class keeps the sup's "T", which is unknown at the
// call (E26, reported in std).
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
