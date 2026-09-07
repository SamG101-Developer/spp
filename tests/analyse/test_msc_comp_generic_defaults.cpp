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
