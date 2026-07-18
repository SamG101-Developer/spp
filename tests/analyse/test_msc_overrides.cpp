#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverrides,
    test_valid_overrides, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f(&self) -> Void { }

        !virtual_method
        !public
        fun f(&self, a: A) -> Void { }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B()
        b.f()
        b.f(A())
        b.f(true, 1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverrides,
    test_valid_overrides_with_generics, R"(
    cls A[T] { }
    cls B[T] { }

    sup [T] A[T] {
        !virtual_method
        !public
        fun f(&self) -> Void { }

        !virtual_method
        !public
        fun f(&self, a: T) -> T { ret T() }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B[S32]()
        b.f()
        let mut x = b.f(1)
        x = 123
        b.f(true, 1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverrides,
    test_valid_overrides_with_generics_complex, R"(
    cls A[T] { }
    cls B[T] { }

    sup [T] A[T] {
        !virtual_method
        !public
        fun f(&self) -> Void { }

        !virtual_method
        !public
        fun f(&self, a: T) -> Vec[T] { ret Vec[T]() }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B[S32]()
        b.f()
        let mut x = b.f(1)
        x = Vec[S32]()
        b.f(true, 1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestOverrides,
    test_valid_coroutine_overrides_with_generics, R"(
    cls A[T] { }
    cls B[T] { }

    sup [T] A[T] {
        !virtual_method
        !public
        cor c(&self) -> Gen[&T, Bool] { }

        !virtual_method
        !public
        cor c(&self, a: T) -> Gen[&T, Bool] { }

        !virtual_method
        !public
        cor c(&self, a: Bool, b: S32) -> Gen[&T, Bool] { }
    }

    sup [T] B[T] ext A[T] {
        cor c(&self) -> Gen[&T, Bool] { }
    }

    fun test() -> Void {
        let b = B[S32]()
        let mut coroutine = b.c(123)
        coroutine.res(false)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverrides,
    test_invalid_override_call,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f(&self) -> Void { }

        !virtual_method
        !public
        fun f(&self, a: A) -> Void { }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B()
        b.f("a")
    }
)");


// A base method that is neither `virtual_method` nor `abstract_method` cannot be overridden.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverrides,
    test_invalid_override_non_virtual_method,
    SppSuperimpositionExtensionNonVirtualMethodOverriddenError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        fun f(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }
)");


// A method defined in an `ext` block must correspond to a method on the base type; a brand-new
// method that overrides nothing is invalid.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestOverrides,
    test_invalid_ext_method_not_on_base,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f(&self) -> Void { }
    }

    sup B ext A {
        fun g(&self) -> Void { }
    }
)");
