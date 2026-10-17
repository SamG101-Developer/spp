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
        std::mem::ops::drop(a)
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
        std::mem::ops::drop(a)
    }
)");

// The two instances are one type to the analysis (variant members are unordered), but two structs to codegen.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentTypeAst,
    test_valid_variant_generic_arguments_in_another_order, R"(
    cls VarArgHolder[T] { }

    fun f(x: VarArgHolder[S32 or Bool]) -> VarArgHolder[Bool or S32] { ret x }
)");
