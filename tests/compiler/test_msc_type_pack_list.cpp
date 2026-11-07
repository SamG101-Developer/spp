#include "test_macros.hpp"

// A type pack is bound to a list of types, not a tuple (pack-design.md phase 11). Each class names its classes by
// parameter, "Tup" too; a pack is only ever given positionally, as the variadic parameter is last.

// A pack given by name is no argument of its parameter: it takes the trailing positionals, so a name adds nothing.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypePackList,
  test_invalid_pack_given_by_name,
  SppArgumentNameInvalidError, R"(
    cls PackNamed[..Ts] { }

    fun f(x: PackNamed[Ts=(S32, Bool)]) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypePackList,
  test_invalid_tuple_items_given_by_name,
  SppArgumentNameInvalidError, R"(
    fun f(x: std::tuple::Tup[Items=(S32, Bool)]) -> Void { std::mem::ops::drop(x) }
)");

// A pack's element that is a tuple is one element: "P[(S32, Bool), U8]" is two elements, not "P[S32, Bool, U8]". A
// call whose argument fits no parameter is E51 ("constraint-failure-on-a-call-is-e51").
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypePackList,
  test_invalid_pack_of_a_tuple_is_not_its_elements,
  SppFunctionCallNoValidSignaturesError, R"(
    cls PackOfTuple[..Ts] { }

    fun g(x: PackOfTuple[(S32, Bool), U8]) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        g(PackOfTuple[S32, Bool, U8]())
    }
)");

// A pack of one empty tuple is not the empty pack.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypePackList,
  test_invalid_pack_of_an_empty_tuple_is_not_the_empty_pack,
  SppFunctionCallNoValidSignaturesError, R"(
    cls PackOfUnit[..Ts] { }

    fun g(x: PackOfUnit[()]) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        g(PackOfUnit())
    }
)");

// A class over a pack after a fixed parameter: its instance binds both, and holds attributes of each.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypePackList,
  test_valid_class_over_a_fixed_parameter_and_a_pack, R"(
    cls FixedThenPack[A, ..Ts] {
        !public a: A
        !public t: Tup[Ts]
    }

    fun f() -> Void {
        let x = FixedThenPack[S32, Bool, U8](a=1_s32, t=(true, 2_u8))
        let y: Tup[Bool, U8] = x.t
        let z: S32 = x.a
        std::mem::ops::drop(x)
    }
)");

// A tuple's own sup block names its pack: a method returning "Tup[Items]" returns the tuple it is called on.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypePackList,
  test_valid_tuple_sup_method_over_its_pack, R"(
    sup [..Items] Tup[Items] {
        !public fun same(self) -> Tup[Items] { ret self }
    }

    fun f() -> Void {
        let t = (1_s32, true).same()
        let u: (S32, Bool) = t
    }
)");

// Nested tuples stay nested: a tuple of a tuple is a one-element tuple.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypePackList,
  test_valid_nested_tuple, R"(
    fun f() -> Void {
        let t: Tup[Tup[S32, Bool]] = ((1_s32, true),)
        let u: (S32, Bool) = t.0
    }
)");

// A variant's members are its pack: flattened and ordered as before.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypePackList,
  test_valid_variant_members_flatten, R"(
    type SomeOrBool = S32 or Bool

    fun f() -> Void {
        let a: SomeOrBool or Str = 1_s32
        let b: Str or Bool or S32 = a
        std::mem::ops::drop(b)
    }
)");

// Positionals after keywords are a variadic parameter's tail, as in Python ("f(x=1, *rest)"): allowed when each lands
// on the variadic parameter, an order error (E18) when one would bind a fixed parameter.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypePackList,
  test_valid_pack_after_a_keyword_generic_argument, R"(
    cls TailAfterKeyword[A, ..Ts] { }

    fun f() -> Void {
        let x = TailAfterKeyword[A=S32, Bool, U8]()
        let y: TailAfterKeyword[S32, Bool, U8] = x
        std::mem::ops::drop(y)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypePackList,
  test_invalid_fixed_parameter_after_a_keyword_generic_argument,
  SppOrderInvalidError, R"(
    cls FixedAfterKeyword[A, B, ..Ts] { }

    fun f(x: FixedAfterKeyword[A=S32, Bool]) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypePackList,
  test_valid_pack_after_a_keyword_call_argument, R"(
    fun g(a: S32, ..xs: S32) -> Void { }

    fun f() -> Void {
        g(a=1_s32, 2_s32, 3_s32)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypePackList,
  test_invalid_fixed_parameter_after_a_keyword_call_argument,
  SppOrderInvalidError, R"(
    fun g(a: S32, b: S32, ..xs: S32) -> Void { }

    fun f() -> Void {
        g(a=1_s32, 2_s32)
    }
)");
