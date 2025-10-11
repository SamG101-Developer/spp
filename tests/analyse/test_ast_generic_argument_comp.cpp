#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericArgumentGroupAst,
    test_invalid_generic_argument_group_duplicate_named_argument,
    SppIdentifierDuplicateError, R"(
    fun f[T, U]() -> std::void::Void { }

    fun g() -> std::void::Void {
        f[T=std::boolean::Bool, T=std::boolean::Bool]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenericArgumentGroupAst,
    test_invalid_generic_argument_group_invalid_argument_order,
    SppOrderInvalidError, R"(
    fun f[T, U]() -> std::void::Void { }

    fun g() -> std::void::Void {
        f[T=std::boolean::Bool, std::boolean::Bool]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentGroupAst,
    test_valid_generic_argument_group_different_names_from_sup_1,
    R"(
    cls A[T] { a: T }

    sup [T] A[T] {
        fun f(&self) -> std::void::Void { }
    }

    fun g() -> std::void::Void {
        let a = A(a=5)
        a.f()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenericArgumentGroupAst,
    test_valid_generic_argument_group_different_names_from_sup_2,
    R"(
    cls A[T] { a: T }
    sup [T] A[T] {
        fun new(&self) -> A[T] { ret A[T]() }
    }

    sup [T] A[T] {
        fun f(&self) -> T { ret self.new().a }
    }

    fun g() -> std::void::Void {
        let a = A(a=5)
        let mut b = a.f()
        b = a.a
    }
)");
