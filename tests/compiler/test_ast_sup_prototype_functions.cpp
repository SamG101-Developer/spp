#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_unknown_type,
    SppIdentifierUnknownError, R"(
    sup A {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_unconstrained_generic_parameter_1,
    SppSuperimpositionUnconstrainedGenericParameterError, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    sup [T] Point { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_unconstrained_generic_parameter_2,
    SppSuperimpositionUnconstrainedGenericParameterError, R"(
    cls Point[T] {
        x: T
        y: T
    }

    sup [T, U] Point[T] { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_optional_generic_parameter,
    SppSuperimpositionOptionalGenericParameterError, R"(
    cls Point[T] {
        x: T
        y: T
    }

    sup [T=Bool] Point[T] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_onto_generic_type, R"(
    sup [T] T {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_invalid_superimposition_functions_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &mut A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_invalid_superimposition_functions_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &A { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_multiple_blocks_same_type, R"(
    cls A { }

    sup A {
        fun f(&self) -> Void { }
    }

    sup A {
        fun g(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_on_builtin_type, R"(
    sup Bool {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_generic_fallthrough_explicit, R"(
    cls BaseClass[T] { }
    sup [T] BaseClass[T] {
        !public
        fun f(&self) -> T { ret T() }
    }

    fun f() -> Void {
        let x = BaseClass[Bool]()
        let mut y = x.f()
        y = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_generic_fallthrough_implicit, R"(
    cls BaseClass[T] { !public a: T }
    sup [T] BaseClass[T] {
        !public
        fun f(&self) -> T { ret T() }
    }

    fun f() -> Void {
        let x = BaseClass(a=false)
        let mut y = x.f()
        y = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_in_an_array_argument, R"(
    cls A[cmp xs: [U64; 2_uz]] { }
    sup [cmp n: U64] A[[n, 2_u64]] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_in_a_repeated_array_argument, R"(
    cls A[cmp xs: [U64; 2_uz]] { }
    sup [cmp n: U64] A[[n; 2_uz]] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_in_an_object_argument, R"(
    cls A[cmp o: Opt[U64]] { }
    sup [cmp n: U64] A[Some(val=n)] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_in_a_call_argument, R"(
    cmp fun f(x: U64) -> U64 { ret x }
    cls A[cmp v: U64] { }
    sup [cmp n: U64] A[f(n)] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_in_a_postfix_operand, R"(
    cls A[cmp v: Bool] { }
    sup [cmp b: Bool] A[b.not] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_in_a_tuple_argument, R"(
    cls A[cmp t: (U64, U64)] { }
    sup [cmp n: U64] A[(n, 1_u64)] {
        !public
        fun get(&self) -> U64 { ret n }
    }

    fun f() -> Void {
        let a = A[(5_u64, 1_u64)]()
        let v = a.get()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_inferred_through_an_array_argument, R"(
    cls A[cmp xs: [U64; 2_uz]] { }
    sup [cmp n: U64] A[[n, 2_u64]] {
        !public
        fun get(&self) -> U64 { ret n }
    }

    fun f() -> Void {
        let a = A[[1_u64, 2_u64]]()
        let v = a.get()
    }
)");

// Todo (red): as for an array, an object comp value is keyed "Opaque" by its spelling, so "Some(val=n)" never unifies
//  with "Some(val=1_u64)" (E26).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    DISABLED_test_valid_sup_prototype_functions_comp_parameter_inferred_through_an_object_argument, R"(
    cls A[cmp o: Opt[U64]] { }
    sup [cmp n: U64] A[Some(val=n)] {
        !public
        fun get(&self) -> U64 { ret n }
    }

    fun f() -> Void {
        let a = A[Some(val=1_u64)]()
        let v = a.get()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_comp_parameter_inferred_through_a_repeated_array_argument, R"(
    cls A[cmp xs: [U64; 2_uz]] { }
    sup [cmp n: U64] A[[n; 2_uz]] {
        !public
        fun get(&self) -> U64 { ret n }
    }

    fun f() -> Void {
        let a = A[[1_u64, 1_u64]]()
        let v = a.get()
    }
)");
