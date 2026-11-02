#include "test_macros.hpp"

// A variadic parameter declares one element ("..b: T") but binds the whole tuple the call collapsed its trailing
// arguments into. Codegen knew that - the pack type is what the parameter is emitted as - but the symbol in the body
// kept the element type, so naming the pack as a value inferred one element while a tuple was passed, and the call
// failed LLVM verification rather than anything earlier.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_used_as_a_value_is_the_tuple, R"(
    fun takes[T](v: T) -> Void {
        std::mem::ops::drop(v)
    }

    fun packed(a: Bool, ..b: S32) -> Void {
        takes(b)
    }

    fun f() -> Void {
        packed(true, 1, 2, 3)
    }
)");

// The pack's element count is part of the instantiation, so two call sites with different counts are different
// tuples rather than one type that happens to be reused.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_packs_of_different_lengths, R"(
    fun packed[T](a: Bool, ..b: T) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        packed(true, 1, 2, 3)
        packed(true, 4, 5)
    }
)");

// A pack over a variadic generic collects whatever it was given, of whatever types.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_over_a_variadic_generic, R"(
    fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        packed(true, 1, false, 2)
    }
)");

// A pack is a value like any other, so a body that never uses it has abandoned it - unless every element is copyable,
// which is what makes a pack of numbers usable without the function discarding it.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_of_non_copyable_elements_not_consumed,
  SppLinearValueNotConsumedError, R"(
    cls T { }

    fun packed(a: Bool, ..b: T) -> Void { }

    fun f() -> Void {
        packed(true, T(), T())
    }
)");

// A pack spread into another call inside a generic body. Today the body is checked with "b" typed as one element
// ("Ts"), so the spread's tuple check fails (E19, function_call_argument_group_ast.cpp). The spread can only be
// expanded once the function is called and "b" is a tuple.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_spread_into_another_variadic_call, R"(
      fun inner[..Us](..c: Us) -> Void {
          std::mem::ops::drop(c)
      }

      fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
          inner(..b)
      }

      fun f() -> Void {
          packed(true, 1, false, 2)
      }
  )");

// Head/tail recursion over a pack, ending at the overload with no pack. This is what "std::format::format(fmt, ..args)"
// needs. It fails on the same E19 as above.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_head_tail_recursion, R"(
      fun walk() -> Void { }

      fun walk[H, ..Ts](head: H, ..tail: Ts) -> Void {
          std::mem::ops::drop(head)
          walk(..tail)
      }

      fun f() -> Void {
          walk(1, false, 2_u64)
      }
  )");

// A pack passed whole is a tuple of unknown length ("Tup[Ts]"), and "..Ts" says nothing about its length, so a
// constraint only pairs meet cannot be shown to hold for every call: the body is rejected (E51, the constraint's E78
// inside it), however it is called. The sup's parameters are "[P, Q]", not "[A, B]", to avoid the unrelated stdlib name
// clash (param_name_shared_with_stdlib).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_passed_whole_to_a_tuple_constrained_function,
  SppFunctionCallNoValidSignaturesError, R"(
      cls IsPair { }
      sup [P, Q] Tup[P, Q] ext IsPair { }

      fun takes_pair[T: IsPair](t: T) -> Void {
          std::mem::ops::drop(t)
      }

      fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
          takes_pair(b)
      }

      fun f() -> Void {
          packed(true, 1, false)
      }
  )");

// The twin of the above: a constraint every tuple meets, whatever its length, does hold for a pack passed whole. This
// is what shows the pack is typed as the tuple it is, rather than the constraint check being switched off.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_passed_whole_to_a_constraint_every_tuple_meets, R"(
      cls AnyTuple { }
      sup [..Us] Tup[Us] ext AnyTuple { }

      fun takes_any[T: AnyTuple](t: T) -> Void {
          std::mem::ops::drop(t)
      }

      fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
          takes_any(b)
      }

      fun f() -> Void {
          packed(true, 1, false)
      }
  )");

// Head/tail recursion with no overload for the empty pack: "walk(..tail)" with "tail" empty would have nothing to call,
// so the body is rejected, whether or not any call would reach the empty case.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_head_tail_recursion_without_a_base_case,
  SppFunctionCallNoValidSignaturesError, R"(
      fun walk[H, ..Ts](head: H, ..tail: Ts) -> Void {
          std::mem::ops::drop(head)
          walk(..tail)
      }

      fun f() -> Void {
          walk(1, false, 2_u64)
      }
  )");
