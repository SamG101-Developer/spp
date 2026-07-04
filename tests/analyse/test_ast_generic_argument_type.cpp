#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericArgumentTypeAst,
    test_invalid_generic_argument_group_duplicate_named_argument,
    SppIdentifierDuplicateError, R"(
    fun f[T, U]() -> Void { }

    fun g() -> Void {
        f[T=Bool, T=Bool]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericArgumentTypeAst,
    test_invalid_generic_argument_group_invalid_argument_order,
    SppOrderInvalidError, R"(
    fun f[T, U]() -> Void { }

    fun g() -> Void {
        f[T=Bool, Bool]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentTypeAst,
    test_valid_generic_argument_group_different_names_from_sup_1,
    R"(
    cls A[T] { !public a: T }

    sup [T] A[T] {
        !public fun f(&self) -> Void { }
    }

    fun g() -> Void {
        let a = A(a=5)
        a.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentTypeAst,
    test_valid_generic_argument_group_different_names_from_sup_2,
    R"(
    cls A[T] { !public a: T }
    sup [T] A[T] {
        !public fun new(&self) -> A[T] { ret A[T]() }
    }

    sup [T] A[T] {
        !public fun f(&self) -> T { ret self.new().a }
    }

    fun g() -> Void {
        let a = A(a=5)
        let mut b = a.f()
        b = a.a
    }
)");
