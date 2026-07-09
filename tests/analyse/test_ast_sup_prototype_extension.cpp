#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_sup_prototype_functions_unknown_type,
    SppIdentifierUnknownError, R"(
    sup A {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_sup_prototype_functions_unknown_supertype,
    SppIdentifierUnknownError, R"(
    sup Bool ext A {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_valid_superimposition_extension_generic_name, R"(
    cls A { }
    sup [T] T ext A { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeExtensionAst,
    DISABLED_test_valid_superimposition_extension_generic_name_more_complex, R"(
    cls CanBorrowRef[T] { }
    sup [T] CanBorrowRef[T] {
        cor borrow_ref(&self) -> Gen[&T] {
            gen self
        }
    }

    cls CanBorrowMut[T] { }
    sup [T] CanBorrowMut[T] {
        cor borrow_mut(&mut self) -> Gen[&mut T] {
            gen self
        }
    }

    sup [T] T ext CanBorrowRef[T] { }
    sup [T] T ext CanBorrowMut[T] { }

    fun f() -> Void {
        let x = 123
        let y = x.borrow_ref()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_generic_superclass,
    SppGenericTypeInvalidUsageError, R"(
    sup [T] Vec[T] ext T { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_unconstrained_generic,
    SppSuperimpositionUnconstrainedGenericParameterError, R"(
    sup [T] Bool ext S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_optional_generic,
    SppSuperimpositionOptionalGenericParameterError, R"(
    sup [T = Bool] Vec[T] ext S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_duplication_superclass,
    SppSuperimpositionDoubleExtensionError, R"(
    sup Bool ext Str { }
    sup Bool ext Str { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_cyclic_extension,
    SppSuperimpositionCyclicExtensionError, R"(
    sup Bool ext Str { }
    sup Str ext Bool { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_self_extension,
    SppSuperimpositionSelfExtensionError, R"(
    cls A { }
    sup A ext A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_1,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        fun g(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_2,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&mut self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_3,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&self, x: Bool = true) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_4,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&self) -> Bool { ret true }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_non_virtual_method_override,
    SppSuperimpositionExtensionNonVirtualMethodOverriddenError, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_type_statement,
    SppSuperimpositionExtensionTypeStatementInvalidError, R"(
    cls A { }

    cls B { }
    sup B ext A {
        type X = Str
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_cmp_statement,
    SppSuperimpositionExtensionCmpStatementInvalidError, R"(
    cls A { }

    cls B { }
    sup B ext A {
        cmp x: Str = "hello world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_cmp_statement_type_mismatch,
    SppSuperimpositionExtensionCmpStatementInvalidError, R"(
    cls A { }
    sup A {
        !public cmp x: Str = "hello world"
    }

    cls B { }
    sup B ext A {
        cmp x: S32 = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_valid_superimposition_extension_cmp_statement_override, R"(
    cls A { }
    sup A {
        !public cmp x: Str = "hello world"
    }

    cls B { }
    sup B ext A {
        cmp x: Str = "goodbye world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &mut A ext S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &A ext S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_supertype_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup A ext &mut S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_invalid_superimposition_extension_supertype_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup A ext &S32 { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_valid_superimposition_extension_generic_variants, R"(
    cls BaseClass[T] { }

    cls A { }
    sup A ext BaseClass[S32] { }
    sup A ext BaseClass[Bool] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_valid_superimposition_extension_stateful, R"(
    cls A { !public a: S32 }
    cls B { !public b: S32 }

    sup A {
        !virtual_method
        !public
        fun f(mut self) -> Void {
            self.a = 100
        }
    }

    sup B ext A {
        fun f(mut self) -> Void {
            self.a = self.b
        }
    }

    fun f() -> Void {
        let b = B(b=200)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupPrototypeExtensionAst,
    test_valid_superimposition_extension_generics_1, R"(
    cls A[T] { !public a: T }
    cls B[T] { !public b: T }

    sup [T] A[T] {
        !virtual_method
        !public
        fun f(mut self) -> Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(mut self) -> Void {
            self.a = self.b
        }
    }

    fun f() -> Void {
        let b = B(b=100)
    }
)");
