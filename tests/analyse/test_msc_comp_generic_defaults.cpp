#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_default_naming_a_self_qualified_constant, R"(
    cls Holder[T] { }

    sup [T] Holder[T] {
        !public cmp fallback: U8 = 7_u8

        !public fun pick[cmp order: U8 = Self::fallback]() -> U8 {
            ret order
        }
    }

    fun f() -> Void {
        let a = Holder[S32]::pick()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_self_qualified_constant_written_out_explicitly, R"(
    cls Holder[T] { }

    sup [T] Holder[T] {
        !public cmp fallback: U8 = 7_u8

        !public fun pick[cmp order: U8 = Self::fallback]() -> U8 {
            ret order
        }
    }

    fun f() -> Void {
        let a = Holder[S32]::pick[Holder[S32]::fallback]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_default_overridden_by_a_literal, R"(
    cls Holder[T] { }

    sup [T] Holder[T] {
        !public cmp fallback: U8 = 7_u8

        !public fun pick[cmp order: U8 = Self::fallback]() -> U8 {
            ret order
        }
    }

    fun f() -> Void {
        let a = Holder[S32]::pick[3_u8]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_literal_default_needs_no_resolution, R"(
    cls Holder[T] { }

    sup [T] Holder[T] {
        !public fun pick[cmp order: U8 = 7_u8]() -> U8 {
            ret order
        }
    }

    fun f() -> Void {
        let a = Holder[S32]::pick()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_default_on_a_method_taking_self, R"(
    cls Holder[T] { }

    sup [T] Holder[T] {
        !public cmp fallback: U8 = 7_u8

        fun new() -> Self { ret Holder[T]() }

        !public fun pick[cmp order: U8 = Self::fallback](&self) -> U8 {
            ret order
        }
    }

    fun f() -> Void {
        let h = Holder[S32]::new()
        let a = h.pick()
        std::mem::ops::drop(h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_two_instantiations_of_one_default, R"(
    cls Holder[T] { }

    sup [T] Holder[T] {
        !public cmp fallback: U8 = 7_u8

        !public fun pick[cmp order: U8 = Self::fallback]() -> U8 {
            ret order
        }
    }

    fun f() -> Void {
        let a = Holder[S32]::pick()
        let b = Holder[Bool]::pick()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_default_calling_a_cmp_function, R"(
    cmp fun three() -> USize { ret 3_uz }

    fun pick[cmp n: USize = three()]() -> USize {
        ret n
    }

    fun f() -> Void {
        let a = pick()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompGenericDefaults,
  test_invalid_default_calling_a_runtime_function,
  SppCompileTimeConstantError, R"(
    fun three() -> USize { ret 3_uz }

    fun pick[cmp n: USize = three()]() -> USize {
        ret n
    }

    fun f() -> Void {
        let a = pick()
    }
)");

// A default naming an earlier parameter is translated from what was written, so it folds to the value the argument
// gives it: analysed in place, "n + 1_uz" had been desugared into a call, which the use substituted into instead.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_default_naming_an_earlier_parameter, R"(
    cls A[cmp n: USize, cmp m: USize = n + 1_uz] { }

    fun f() -> Void {
        let a = A[3_uz]()
        let b: A[3_uz, 4_uz] = a
        std::mem::ops::drop(b)
    }
)");

// Chained: a default naming another default, and each taking a written argument over its default.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCompGenericDefaults,
  test_valid_default_naming_another_default, R"(
    cls B[T, cmp n: USize = 2_uz, cmp m: USize = n * 2_uz] { }

    fun f() -> Void {
        let a = B[S32]()
        let b: B[S32, 2_uz, 4_uz] = a
        std::mem::ops::drop(b)
        let c = B[S32, 5_uz]()
        let d: B[S32, 5_uz, 10_uz] = c
        std::mem::ops::drop(d)
    }
)");

// And the value it folds to is the one the type has.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompGenericDefaults,
  test_invalid_default_naming_an_earlier_parameter_other_value,
  SppTypeMismatchError, R"(
    cls A[cmp n: USize, cmp m: USize = n + 1_uz] { }

    fun f() -> Void {
        let a = A[3_uz]()
        let b: A[3_uz, 5_uz] = a
        std::mem::ops::drop(b)
    }
)");
