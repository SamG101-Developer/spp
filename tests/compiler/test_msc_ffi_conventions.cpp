#include "test_macros.hpp"

// "!conv" names the convention a class is laid out by, or a function is called with; "!ffi" implies "C". A name no
// convention has is an unknown identifier, as is one that does not fit what it is on: a foreign convention on a
// function implemented in s++, or a non-foreign one on an "!ffi" function or class.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_unknown_convention_on_a_function,
  SppIdentifierUnknownError, R"(

    !conv("fast")
    fun g() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_unknown_convention_on_a_class,
  SppIdentifierUnknownError, R"(

    !conv("c")
    cls A { !public a: S32 }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_foreign_convention_on_a_function_defined_in_spp,
  SppIdentifierUnknownError, R"(

    !conv("C")
    fun g() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_spp_convention_on_an_ffi_function,
  SppIdentifierUnknownError, R"(

    !ffi(symbol="zz_absent")
    !conv("spp")
    fun g() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_spp_convention_on_an_ffi_class,
  SppIdentifierUnknownError, R"(

    !ffi
    !conv("spp")
    cls A { !public a: S32 }
)");

// Every attribute of an "!ffi" class is one a C struct can hold.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_ffi_class_holding_a_class_laid_out_the_spp_way,
  SppFfiAttributeTypeError, R"(
    cls Inner {
      !public a: U8
      !public b: U64
    }

    !ffi
    cls A { !public x: Inner }

    fun f(a: A) -> A { ret a }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_ffi_class_holding_a_variant,
  SppFfiAttributeTypeError, R"(
    !ffi
    cls A { !public x: S32 or Bool }

    fun f(a: A) -> A { ret a }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_ffi_class_holding_a_function,
  SppFfiAttributeTypeError, R"(
    !ffi
    cls A { !public x: FunRef[(S32,), S32] }

    fun f(a: A) -> A { ret a }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiConventions,
  test_invalid_generic_ffi_class_instantiated_with_a_class_laid_out_the_spp_way,
  SppFfiAttributeTypeError, R"(
    cls Inner {
      !public a: U8
      !public b: U64
    }

    !ffi
    cls A[T] { !public x: T }

    fun f(a: A[Inner]) -> A[Inner] { ret a }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiConventions,
  test_valid_ffi_class_of_numbers_pointers_and_c_classes,
  R"(

    !ffi
    cls Inner {
      !public a: U8
      !public b: U64
    }

    !conv("C")
    cls Tagged {
      !public tag: U16
      !public val: F64
    }

    !ffi
    cls A {
      !public n: S32
      !public inner: Inner
      !public tagged: Tagged
      !public many: [Inner; 2_uz]
      !public pair: (U8, U32)
      !public flag: Bool
    }

    fun f(a: A) -> A { ret a }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiConventions,
  test_valid_generic_ffi_class_instantiated_with_numbers,
  R"(
    !ffi
    cls A[T] {
      !public x: T
      !public y: U8
    }

    fun f(a: A[U64]) -> A[U64] { ret a }
)");

// Only "!ffi" restricts the attributes: "!conv("C")" alone fixes the order and nothing else.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiConventions,
  test_valid_c_convention_class_holding_anything,
  R"(

    !conv("C")
    cls A {
      !public s: Str
      !public n: U8
    }

    fun f(a: A) -> A { ret a }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiConventions,
  test_valid_spp_convention_named_on_a_function_and_a_class,
  R"(

    !conv("spp")
    cls A {
      !public a: U8
      !public b: U64
    }

    !conv(name="spp")
    fun g(a: A) -> A { ret a }
)");

// "!ffi !conv("C")" says the same thing twice; and an "!ffi" function with no "symbol" binds to its own name.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiConventions,
  test_valid_ffi_function_naming_c_and_its_own_symbol,
  R"(

    !ffi
    !conv("C")
    fun labs(x: S64) -> S64 { }
)");
