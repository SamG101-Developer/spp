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
        fun f(&self, a: A) -> Void {
            std::mem::ops::drop(a)
        }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    fun test_fn() -> Void {
        let b = B()
        b.f()
        b.f(A())
        b.f(true, 1)
        std::mem::ops::drop(b)
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
        fun f(&self, a: T) -> T { ret a }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(&self) -> Void { }
    }

    fun test_fn() -> Void {
        let b = B[S32]()
        b.f()
        let mut x = b.f(1)
        x = 123
        b.f(true, 1)
        std::mem::ops::drop(b)
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
        fun f(&self, a: T) -> Vec[T] {
            std::mem::ops::drop(a)
            ret Vec[T]()
        }

        !virtual_method
        !public
        fun f(&self, a: Bool, b: S32) -> Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(&self) -> Void { }
    }

    fun test_fn() -> Void {
        let b = B[S32]()
        b.f()
        let mut x = b.f(1)
        x = Vec[S32]()
        b.f(true, 1)

        std::mem::ops::drop(b)
        drop(x)
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
        cor c(&self, a: T) -> Gen[&T, Bool] {
            std::mem::ops::drop(a)
        }

        !virtual_method
        !public
        cor c(&self, a: Bool, b: S32) -> Gen[&T, Bool] { }
    }

    sup [T] B[T] ext A[T] {
        cor c(&self) -> Gen[&T, Bool] { }
    }

    fun test_fn() -> Void {
        let b = B[S32]()
        {
            let mut coroutine = b.c(123)
            let v = coroutine.res(false)
        }
        std::mem::ops::drop(b)
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

    fun test_fn() -> Void {
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

// An override may narrow what the method it overrides returns: whatever it returns is still what the base promises.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverrides,
  test_valid_override_narrows_the_return_type, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f(&self) -> S32 or Bool { ret true }
    }

    sup B ext A {
        fun f(&self) -> S32 { ret 1_s32 }
    }
)");

// An override may not widen it: a caller of the base's method would be handed what the base never returns.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverrides,
  test_invalid_override_widens_the_return_type,
  SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f(&self) -> S32 { ret 1_s32 }
    }

    sup B ext A {
        fun f(&self) -> S32 or Bool { ret 1_s32 }
    }
)");

// Nor widen it inside an argument ("Indexed[&T or None]" for "Indexed[&T]"), which the old check accepted by reading
// the two return types the wrong way round.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverrides,
  test_invalid_override_widens_a_return_type_argument,
  SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f(&self) -> Opt[S32] { ret None }
    }

    sup B ext A {
        fun f(&self) -> Opt[S32 or Bool] { ret None }
    }
)");

// A signature is the same up to renaming its own generics: the override's are the base's, by position.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverrides,
  test_valid_override_renames_its_generics, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f[G: Copy](&self, g: G) -> G { ret g }
    }

    sup B ext A {
        fun f[H: Copy](&self, g: H) -> H { ret g }
    }
)");

// By position, so swapping what two generics stand for is another signature.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverrides,
  test_invalid_override_swaps_its_generics,
  SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        !public
        fun f[G: Copy, K: Copy](&self, g: G, k: K) -> G { ret g }
    }

    sup B ext A {
        fun f[G: Copy, K: Copy](&self, g: K, k: G) -> G { ret k }
    }
)");
