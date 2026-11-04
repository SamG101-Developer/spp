#include "test_macros.hpp"

// A variadic parameter declares one element ("..b: T") but binds the whole tuple the call collapsed its trailing
// arguments into. A pack is no value, so it is collected into one to be passed on ("(..b)").
// Red until phase 4: "(..b)" of a pack of one element type ("..b: S32") has no type in a template yet.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_collected_into_a_tuple_and_passed_on, R"(
    fun takes[T](v: T) -> Void {
        std::mem::ops::drop(v)
    }

    fun packed(a: Bool, ..b: S32) -> Void {
        takes((..b))
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
        std::mem::ops::drop(..b)
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
        std::mem::ops::drop(..b)
    }

    fun f() -> Void {
        packed(true, 1, false, 2)
    }
)");

// Each element of a pack is a value, so a body that never uses them has abandoned them - unless every element is
// copyable, which is what makes a pack of numbers usable without the function discarding it.
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
          std::mem::ops::drop(..c)
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

// A pack collected into a tuple ("(..b)") is a tuple of unknown length ("Tup[Ts]"), and "..Ts" says nothing about its
// length, so a constraint only pairs meet cannot be shown to hold for every call: the body is rejected (E51, the
// constraint's E78 inside it), however it is called. The sup's parameters are "[P, Q]", not "[A, B]", to avoid the
// unrelated stdlib name clash (param_name_shared_with_stdlib).
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
          takes_pair((..b))
      }

      fun f() -> Void {
          packed(true, 1, false)
      }
  )");

// The twin of the above: a constraint every tuple meets, whatever its length, does hold for a pack collected into a
// tuple. This is what shows the tuple is typed as what it is, rather than the constraint check being switched off.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_passed_whole_to_a_constraint_every_tuple_meets, R"(
      cls AnyTuple { }
      sup [..Us] Tup[Us] ext AnyTuple { }

      fun takes_any[T: AnyTuple](t: T) -> Void {
          std::mem::ops::drop(t)
      }

      fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
          takes_any((..b))
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

// Spreading with a convention: the convention written on the spread is given to every element ("&..t" is
// "&t.0, &t.1", "&mut ..t" is "&mut t.0, &mut t.1"), whether the tuple is owned or itself borrowed. A plain spread
// ("..t") passes each element as it is, so it moves them out of an owned tuple, copies them out of a borrowed one when
// they are Copy, and cannot move a non-Copy one out of a borrow.

// Red until a pack of borrows works: "..xs: &Ts" is read as a borrow of the whole tuple "Ts" binds to, not as a
// tuple of borrows, so even "take_refs(&a, &b)" fails (E51). See the Todo in "BindSlots" (overload_resolution.cpp).
// A borrowed tuple spread into a pack of borrows: each element is passed as a borrow, so a non-Copy element is not
// moved out of the borrow. Needed by "sup [..Items: Format] Tup[Items] ext std::format::Format", which only has
// "&self".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_borrowed_tuple_spread_into_borrowed_pack, R"(
      fun take_refs[..Ts](..xs: &Ts) -> Void { }

      fun give(t: &Tup[Str, S32]) -> Void {
          take_refs(&..t)
      }
  )");

// An owned tuple spread as borrows is only borrowed, so it is still the caller's afterwards.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_owned_tuple_spread_as_borrows_is_not_moved, R"(
      fun refs(a: &Str, b: &S32) -> Void { }

      fun f() -> Void {
          let t = (Str::from("a"), 1_s32)
          refs(&..t)
          refs(&..t)
          std::mem::ops::drop(t)
      }
  )");

// A plain spread of a borrowed tuple would move its non-Copy element out of the borrow.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_plain_spread_of_a_borrowed_tuple_moves_out_of_the_borrow,
  SppMoveFromBorrowedMemoryError, R"(
      fun take_vals[..Ts](..xs: Ts) -> Void {
          std::mem::ops::drop(..xs)
      }

      fun give(t: &Tup[Str, S32]) -> Void {
          take_vals(..t)
      }
  )");

// A plain spread of a borrowed tuple of Copy elements copies them out.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_plain_spread_of_a_borrowed_tuple_of_copy_elements, R"(
      fun take_vals[..Ts](..xs: Ts) -> Void {
          std::mem::ops::drop(..xs)
      }

      fun give(t: &Tup[S32, Bool]) -> Void {
          take_vals(..t)
      }
  )");

// Each element is a place of its own, so mutable borrows of every element at once do not overlap.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_owned_tuple_spread_as_mutable_borrows, R"(
      fun muts(a: &mut Str, b: &mut S32) -> Void { }

      fun f() -> Void {
          let mut t = (Str::from("a"), 1_s32)
          muts(&mut ..t)
          std::mem::ops::drop(t)
      }
  )");

// A tuple borrowed immutably cannot have its elements borrowed mutably.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_mutable_spread_of_an_immutably_borrowed_tuple,
  SppInvalidMutationError, R"(
      fun muts(a: &mut Str, b: &mut S32) -> Void { }

      fun give(t: &Tup[Str, S32]) -> Void {
          muts(&mut ..t)
      }
  )");

// A tuple borrowed mutably can have every element borrowed mutably, as a reborrow.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_mutable_spread_of_a_mutably_borrowed_tuple, R"(
      fun muts(a: &mut Str, b: &mut S32) -> Void { }

      fun give(t: &mut Tup[Str, S32]) -> Void {
          muts(&mut ..t)
      }
  )");

// An empty tuple spread as borrows passes nothing.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_empty_tuple_spread_as_borrows, R"(
      fun none() -> Void { }

      fun f() -> Void {
          let t = ()
          none(&..t)
      }
  )");

// The elements' borrows are held by the call, so the call cannot also move the tuple they borrow from: the same
// error as "take(&t.0, &t.1, t)" written out (moving first, "take(t, &..t)", is E5, as written out too).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_tuple_moved_and_spread_as_borrows_in_one_call,
  SppMemoryOverlapUsageError, R"(
      fun take(a: &Str, b: &S32, whole: Tup[Str, S32]) -> Void {
          std::mem::ops::drop(whole)
      }

      fun f() -> Void {
          let t = (Str::from("a"), 1_s32)
          take(&..t, t)
      }
  )");

// Red until a pack of borrows works: "..xs: &Ts" is read as a borrow of the whole tuple "Ts" binds to, not as a
// tuple of borrows, so even "take_refs(&a, &b)" fails (E51). See the Todo in "BindSlots" (overload_resolution.cpp).
// A template's pack spread as borrows: each length's call borrows "b.0", "b.1", ..., and the pack is still the
// function's to consume afterwards.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_spread_as_borrows_in_a_template, R"(
      fun take_refs[..Us](..xs: &Us) -> Void { }

      fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
          take_refs(&..b)
          std::mem::ops::drop(..b)
      }

      fun f() -> Void {
          packed(true, Str::from("a"), 1, false)
      }
  )");

// Red until a pack of borrows works: "..xs: &Ts" is read as a borrow of the whole tuple "Ts" binds to, not as a
// tuple of borrows, so even "take_refs(&a, &b)" fails (E51). See the Todo in "BindSlots" (overload_resolution.cpp).
// The formatter's shape: a method of every tuple, taking "&self", spreads its elements as borrows into a pack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_self_tuple_spread_as_borrows_in_a_tuple_sup, R"(
      fun take_refs[..Us](..xs: &Us) -> Void { }

      sup [..Items] Tup[Items] {
          !public
          fun show(&self) -> Void {
              take_refs(&..self)
          }
      }

      fun f() -> Void {
          let t = (Str::from("a"), 1_s32)
          t.show()
          std::mem::ops::drop(t)
      }
  )");

// The target behaviour of ".triages/docs_open/pack-design.md": a function's pack is a list of values, not a tuple.
// Red until the phase named on each; a whole use of a pack and "Ts" as a type get their errors, and tests, in phase 2.

// Red until phase 4: a pack's convention is each element's, so "&Ts" given "&Str, &S32" binds "Ts" to "(Str, S32)".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_borrowed_pack_of_a_variadic_generic, R"(
      fun show[..Ts](..xs: &Ts) -> Void { }

      fun f() -> Void {
          let a = Str::from("a")
          let b = 1_s32
          show(&a, &b)
          std::mem::ops::drop(a)
      }
  )");

// Red until phase 4: the same for a pack of one type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_borrowed_pack_of_one_type, R"(
      fun show(..xs: &Str) -> Void { }

      fun f() -> Void {
          let a = Str::from("a")
          let b = Str::from("b")
          show(&a, &b)
          std::mem::ops::drop(a)
          std::mem::ops::drop(b)
      }
  )");

// Red until phase 4: and for mutable borrows, each of its own place.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_mutably_borrowed_pack, R"(
      fun bump(..xs: &mut S32) -> Void { }

      fun f() -> Void {
          let mut a = 1_s32
          let mut b = 2_s32
          bump(&mut a, &mut b)
      }
  )");

// A borrowed pack walked head and tail, as the owned one is: the remainder call "walk(tail.0, ..tail)" binds the
// rest of the borrowed pack to the borrowed pack parameter.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_borrowed_pack_head_tail_recursion, R"(
      fun walk() -> Void { }

      fun walk[H, ..Ts](head: &H, ..tail: &Ts) -> Void {
          walk(..tail)
      }

      fun f() -> Void {
          let a = Str::from("a")
          let b = 1_s32
          walk(&a, &b)
          std::mem::ops::drop(a)
      }
  )");

// Red until phase 4: a borrowed pack spread on, as it is, into another borrowed pack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_borrowed_pack_forwarded_to_a_borrowed_pack, R"(
      fun inner[..Us](..ys: &Us) -> Void { }

      fun outer[..Ts](..xs: &Ts) -> Void {
          inner(..xs)
      }

      fun f() -> Void {
          let a = Str::from("a")
          outer(&a, &a)
          std::mem::ops::drop(a)
      }
  )");

// A borrowed pack given owned values mismatches its convention, as "f[T](x: &T)" given "a" does.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_borrowed_pack_given_owned_values,
  SppFunctionCallNoValidSignaturesError, R"(
      fun show[..Ts](..xs: &Ts) -> Void { }

      fun f() -> Void {
          show(Str::from("a"), 1_s32)
      }
  )");

// Red until phase 4: an owned pack given a borrow mismatches its convention (E51), as "f[T](x: T)" given "&a" does. Today
// "BindSlots" drops the arguments' conventions, so with Copy elements the call is accepted as "take(a, 2_s32)".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_owned_pack_given_a_borrow,
  SppFunctionCallNoValidSignaturesError, R"(
      fun eat() -> Void { }

      fun eat[H, ..Ts](head: H, ..tail: Ts) -> Void {
          std::mem::ops::drop(head)
          eat(..tail)
      }

      fun take[..Ts](..xs: Ts) -> Void {
          eat(..xs)
      }

      fun f() -> Void {
          let a = 1_s32
          take(&a, 2_s32)
      }
  )");

// Red until phase 2: "(..xs)" builds a tuple from an owned pack, moving its elements (D3), typed "Tup[Ts]".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_owned_pack_to_a_tuple, R"(
      fun collect[..Ts](..xs: Ts) -> Tup[Ts] {
          ret (..xs)
      }

      fun f() -> Void {
          let t = collect(Str::from("a"), 1_s32)
          std::mem::ops::drop(t)
      }
  )");

// Red until phase 2: "(..xs)" of a borrowed pack would be a tuple of borrows (D3).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_borrowed_pack_to_a_tuple,
  SppSecondClassBorrowViolationError, R"(
      fun collect[..Ts](..xs: &Ts) -> Void {
          let t = (..xs)
      }

      fun f() -> Void {
          let a = Str::from("a")
          collect(&a)
          std::mem::ops::drop(a)
      }
  )");

// Red until phase 2: a tuple literal takes spread elements, of a tuple as of a pack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_tuple_literal_with_a_spread_element, R"(
      fun f() -> Void {
          let t = (1_s32, true)
          let u = (0_u8, ..t)
          let v = (..u)
      }
  )");

// Red until phase 6: a pack is consumed by spreading it into the variadic "drop" (D4), with no tuple built.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_consumed_by_the_variadic_drop, R"(
      fun take[..Ts](..xs: Ts) -> Void {
          std::mem::ops::drop(..xs)
      }

      fun f() -> Void {
          take(Str::from("a"), Str::from("b"))
      }
  )");

// Each element is its own value: one moved out leaves the pack partly moved, so it cannot be spread whole.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_element_moved_then_spread,
  SppPartiallyInitializedMemoryUseError, R"(
      fun eat() -> Void { }

      fun eat[H, ..Ts](head: H, ..tail: Ts) -> Void {
          std::mem::ops::drop(head)
          eat(..tail)
      }

      fun take[..Ts](..xs: Ts) -> Void {
          let a = xs.0
          std::mem::ops::drop(a)
          eat(..xs)
      }

      fun f() -> Void {
          take(Str::from("a"), Str::from("b"))
      }
  )");

// Each element is its own value: one left unconsumed is not consumed (E94), whichever it is.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_element_left_unconsumed,
  SppLinearValueNotConsumedError, R"(
      fun take[..Ts](..xs: Ts) -> Void {
          std::mem::ops::drop(xs.0)
      }

      fun f() -> Void {
          take(Str::from("a"), Str::from("b"))
      }
  )");

// D5: a comp pack stays a tuple. Used whole it is its tuple value; indexed, see
// "test_valid_variadic_comp_parameter_element_access".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_comp_pack_used_whole_as_a_tuple, R"(
      fun g[cmp ..n: Bool]() -> Void {
          let t = n
      }

      fun f() -> Void {
          g[true, false]()
      }
  )");

// D5: a comp pack spreads as a tuple does. In the template it is open, so the call is resolved per length, as a
// function pack's is: into a variadic parameter, after fixed arguments too. open-bugs.md #51.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_comp_pack_spread, R"(
      fun h(a: Bool, ..bs: Bool) -> Void { }

      fun k(..bs: Bool) -> Void { }

      fun g[cmp ..n: Bool]() -> Void {
          k(..n)
          h(true, ..n)
      }

      fun f() -> Void {
          g[true, false]()
          g[true]()
          g()
      }
  )");

// A template's comp pack is declared as a function pack is (next-steps.md N14.1): an open tuple of a hidden pack of its
// element, so the folds take it as they take a function pack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_folds_over_a_comp_pack, R"(
      fun twice(t: S32) -> S32 {
          ret t + t
      }

      fun g[cmp ..n: S32]() -> S32 {
          let r = twice(n)..
          ret (.. + n) + (.. + r)
      }

      fun f() -> Void {
          let a = g[1_s32, 2_s32]()
          let b = g[3_s32, 4_s32, 5_s32]()
      }
  )");

// D5: a comp pack used whole is its tuple, open in its template, so an element reads off it.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_comp_pack_used_whole_reads_an_element, R"(
      fun g[cmp ..n: Bool]() -> Bool {
          let t = n
          ret t.0
      }

      fun f() -> Void {
          let a = g[true, false]()
      }
  )");

// An open comp pack's spread stands for every length, as a function pack's does: a callee of two fixed parameters has
// no call for the lengths below two.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_comp_pack_spread_into_fixed_parameters,
  SppFunctionCallNoValidSignaturesError, R"(
      fun h(a: Bool, b: Bool) -> Void { }

      fun g[cmp ..n: Bool]() -> Void {
          h(..n)
      }

      fun f() -> Void {
          g[true, false]()
      }
  )");

// A call fold evaluates its receiver and its unfolded arguments once, as a binary fold evaluates its operand
// (open-bugs.md #59, next-steps.md N14.6): every call borrows the one receiver.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_call_fold_borrows_its_receiver_once, R"(
      cls C { }

      sup C {
          !public fun look(&self, x: S32) -> S32 { ret x }
      }

      fun make() -> C {
          ret C()
      }

      fun g(..xs: S32) -> Void {
          let r = make().look(xs)..
      }

      fun f() -> Void {
          g(1, 2)
      }
  )");

// Evaluated once, a receiver a method consumes is consumed by the first call; the second uses it moved.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_call_fold_consumes_its_receiver_twice,
  SppUninitializedMemoryUseError, R"(
      cls C { }

      sup C {
          !public fun take(self, x: S32) -> Void {
              std::mem::ops::drop(self)
          }
      }

      fun make() -> C {
          ret C()
      }

      fun g(..xs: S32) -> Void {
          make().take(xs)..
      }

      fun f() -> Void {
          g(1, 2)
      }
  )");

// So is an unfolded argument each call takes by value.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_call_fold_moves_an_unfolded_argument_twice,
  SppUninitializedMemoryUseError, R"(
      fun h(x: S32, s: Str) -> Void {
          std::mem::ops::drop(s)
      }

      fun g(..xs: S32) -> Void {
          h(xs, Str::from("a"))..
      }

      fun f() -> Void {
          g(1, 2)
      }
  )");

// D7: a spread into a variadic parameter that is not the last positional argument would take the arguments after it.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_spread_not_the_last_positional_argument,
  SppSpreadNotLastError, R"(
      fun inner[..Us](..ys: Us) -> Void { }

      fun outer[..Ts](..xs: Ts) -> Void {
          inner(..xs, 1_s32)
      }

      fun f() -> Void {
          outer(1_s32)
      }
  )");

// Red until phase 3: each whole use of a pack is "a pack is not a value" (D1).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_passed_whole,
  SppPackNotValueError, R"(
      fun takes[T](v: T) -> Void {
          std::mem::ops::drop(v)
      }

      fun packed[..Ts](..xs: Ts) -> Void {
          takes(xs)
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_returned_whole,
  SppPackNotValueError, R"(
      fun packed[..Ts](..xs: Ts) -> Tup[Ts] {
          ret xs
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_bound_whole,
  SppPackNotValueError, R"(
      fun packed(..xs: S32) -> Void {
          let y = xs
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_compared_whole,
  SppPackNotValueError, R"(
      fun packed(..xs: S32) -> Bool {
          ret xs == xs
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_matched_whole,
  SppPackNotValueError, R"(
      fun packed(..xs: S32) -> Void {
          case xs of {
              is (a, b) { }
              else { }
          }
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_captured,
  SppPackNotValueError, R"(
      fun packed(..xs: S32) -> Void {
          let c = (caps xs) 1_s32
          std::mem::ops::drop(c)
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_stored_in_a_field,
  SppPackNotValueError, R"(
      cls Holder[T] {
          !public v: T
      }

      fun packed(..xs: S32) -> Void {
          let h = Holder(v=xs)
          std::mem::ops::drop(h)
      }
  )");

// "Ts" alone is "a pack is not a type" (D2), as a parameter's type and as a return type. "..xs: Ts" is the pack's
// element type, so is no whole use.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_type_pack_as_a_parameter_type,
  SppPackNotTypeError, R"(
      fun packed[..Ts](t: Ts) -> Void {
          std::mem::ops::drop(t)
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_type_pack_as_a_return_type,
  SppPackNotTypeError, R"(
      fun packed[..Ts](..xs: Ts) -> Ts {
          ret (..xs)
      }
  )");

// A tuple that is no place is evaluated once, into a temporary the elements are read off (open-bugs.md #52; that it
// runs once is "tests/codegen/check-spread-operand.sh").
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_spread_of_a_temporary_tuple, R"(
      fun g() -> (S32, S32) {
          ret (1_s32, 2_s32)
      }

      fun sum(a: S32, b: S32) -> S32 { ret a + b }

      fun f() -> Void {
          let n = sum(..g())
          let t = (0_s32, ..g())
      }
  )");

// A temporary of linear elements spread by move gives each away, which consumes it.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_spread_of_a_temporary_tuple_of_owned_values, R"(
      fun strs() -> (Str, Str) {
          ret (Str::from("a"), Str::from("b"))
      }

      fun take(a: Str, b: Str) -> Void {
          std::mem::ops::drop(a)
          std::mem::ops::drop(b)
      }

      fun f() -> Void {
          take(..strs())
      }
  )");

// Spread as borrows, the temporary is only borrowed, so its linear elements are not consumed: as for any borrowed
// linear temporary ("TestLinearTemporaries").
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_borrowed_spread_of_a_temporary_tuple_of_owned_values,
  SppLinearValueNotConsumedError, R"(
      fun strs() -> (Str, Str) {
          ret (Str::from("a"), Str::from("b"))
      }

      fun look(a: &Str, b: &Str) -> Void { }

      fun f() -> Void {
          look(&..strs())
      }
  )");

// Phase 5: forwarding an open pack binds the callee's pack to it. A pack of one element type forwarded into a pack of
// the same element type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_of_one_type_forwarded_to_a_pack_of_that_type, R"(
      fun inner(..ys: S32) -> Void { }

      fun outer(..xs: S32) -> Void {
          inner(..xs)
      }

      fun f() -> Void {
          outer(1_s32, 2_s32)
      }
  )");

// ...into a pack over a type pack, whose type then follows it: "g" returns the tuple of what it was given.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_of_one_type_forwarded_to_a_type_pack, R"(
      fun g[..Us](..ys: Us) -> Tup[Us] {
          ret (..ys)
      }

      fun f[T](..xs: T) -> Void {
          let t = g(..xs)
          std::mem::ops::drop(t)
      }

      fun h() -> Void {
          f(1_s32, 2_s32)
      }
  )");

// ...into a constrained type pack, which every element meets as its element type does.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_pack_of_one_type_forwarded_to_a_constrained_type_pack, R"(
      fun g[..Us: std::copy::Copy](..ys: Us) -> Void { }

      fun f[T: std::copy::Copy](..xs: T) -> Void {
          g(..xs)
      }

      fun h() -> Void {
          f(1_s32, 2_s32)
      }
  )");

// ...but not into a pack of another element type.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_of_one_type_forwarded_to_a_pack_of_another_type,
  SppFunctionCallNoValidSignaturesError, R"(
      fun inner(..ys: Bool) -> Void { }

      fun outer(..xs: S32) -> Void {
          inner(..xs)
      }

      fun f() -> Void {
          outer(1_s32, 2_s32)
      }
  )");

// Phase 6: memory. A borrowed pack's elements are borrows the call holds, each its own argument: two mutable borrows of
// one place are an overlap, as for any two arguments.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_mutably_borrowed_pack_given_one_place_twice,
  SppMemoryOverlapUsageError, R"(
      fun bump[..Ts](..xs: &mut Ts) -> Void { }

      fun f() -> Void {
          let mut a = 1_s32
          bump(&mut a, &mut a)
      }
  )");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_mutably_borrowed_pack_of_distinct_places, R"(
      fun bump[..Ts](..xs: &mut Ts) -> Void { }

      fun f() -> Void {
          let mut a = 1_s32
          let mut b = Str::from("b")
          bump(&mut a, &mut b)
          std::mem::ops::drop(b)
      }
  )");

// ...and across the pack and another argument.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_mutably_borrowed_pack_overlapping_another_argument,
  SppMemoryOverlapUsageError, R"(
      fun bump[..Ts](first: &S32, ..xs: &mut Ts) -> Void { }

      fun f() -> Void {
          let mut a = 1_s32
          bump(&a, &mut a)
      }
  )");

// A spread by move consumes every element, so a second spread uses moved values.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_spread_by_move_twice,
  SppUninitializedMemoryUseError, R"(
      fun take[..Ts](..xs: Ts) -> Void {
          std::mem::ops::drop(..xs)
          std::mem::ops::drop(..xs)
      }

      fun f() -> Void {
          take(Str::from("a"))
      }
  )");

// Each element is a value of its own: one moved twice is a use of a moved value.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_pack_element_moved_twice,
  SppUninitializedMemoryUseError, R"(
      fun take[..Ts](..xs: Ts) -> Void {
          let a = xs.0
          let b = xs.0
          std::mem::ops::drop(a)
          std::mem::ops::drop(b)
      }

      fun f() -> Void {
          take(Str::from("a"))
      }
  )");

// Phase 7: folds over packs. A binary fold over a pack is checked in the template on its element type, by induction:
// a fold of two and of three elements agree on their type, so every length of two or more does.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_binary_fold_over_a_pack_of_one_concrete_type, R"(
      fun total(..xs: S32) -> S32 {
          ret .. + xs
      }

      fun f() -> Void {
          let t = total(1_s32, 2_s32, 3_s32)
      }
  )");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_right_binary_fold_over_a_pack, R"(
      fun total(..xs: S32) -> S32 {
          ret xs + ..
      }

      fun f() -> Void {
          let t = total(1_s32, 2_s32)
      }
  )");

// An unconstrained generic element has no "+" ("add" is unknown on it, E26).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_binary_fold_over_a_pack_of_an_unconstrained_generic,
  SppIdentifierUnknownError, R"(
      fun total[T](..xs: T) -> T {
          ret .. + xs
      }
  )");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_binary_fold_over_a_pack_of_an_addable_generic, R"(
      fun total[T: std::ops::add::Add](..xs: T) -> T {
          ret .. + xs
      }

      fun f() -> Void {
          let t = total(1_s32, 2_s32, 3_s32)
      }
  )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_binary_fold_over_an_unconstrained_type_pack,
  SppIdentifierUnknownError, R"(
      fun total[..Ts](..xs: Ts) -> Void {
          let t = .. + xs
          std::mem::ops::drop(t)
      }
  )");

// Each element of "..Ts: Add" adds with itself, so a pack of one type folds.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_binary_fold_over_an_addable_type_pack_of_one_type, R"(
      fun total[..Ts: std::ops::add::Add](..xs: Ts) -> Void {
          let t = .. + xs
          std::mem::ops::drop(t)
      }

      fun f() -> Void {
          total(1_s32, 2_s32)
      }
  )");

// A fold needs two elements; an instance of one has nothing to combine.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_binary_fold_over_a_pack_of_one_element,
  SppInvalidBinaryFoldExpressionError, R"(
      fun total(..xs: S32) -> S32 {
          ret .. + xs
      }

      fun f() -> Void {
          let t = total(1_s32)
      }
  )");

// A pack named in its own constraint stands for each other element there ("..Ts: Add[Ts]": every element adds with
// every element), so a fold over a pack of mixed types is proven in the template and checked whole at the call.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_binary_fold_over_a_pairwise_addable_type_pack_of_one_type, R"(
      fun total[..Ts: std::ops::add::Add[Ts]](..xs: Ts) -> Void {
          let t = .. + xs
          std::mem::ops::drop(t)
      }

      fun f() -> Void {
          total(1_s32, 2_s32, 3_s32)
      }
  )");

// "S32" does not add a "U8": rejected at the call, not inside the instance.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_binary_fold_over_a_pairwise_addable_type_pack_of_mixed_types,
  SppFunctionCallNoValidSignaturesError, R"(
      fun total[..Ts: std::ops::add::Add[Ts]](..xs: Ts) -> Void {
          let t = .. + xs
          std::mem::ops::drop(t)
      }

      fun f() -> Void {
          total(1_s32, 2_u8)
      }
  )");

// Two types that add with each other, both ways, fold mixed.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_binary_fold_over_a_pairwise_addable_type_pack_of_mixed_types, R"(
      cls A { }
      cls B { }

      sup A ext std::ops::add::Add[A, A] {
          fun add(self, that: A) -> A {
              let A() = that
              ret self
          }
      }

      sup A ext std::ops::add::Add[B, A] {
          fun add(self, that: B) -> A {
              let B() = that
              ret self
          }
      }

      sup B ext std::ops::add::Add[A, B] {
          fun add(self, that: A) -> B {
              let A() = that
              ret self
          }
      }

      sup B ext std::ops::add::Add[B, B] {
          fun add(self, that: B) -> B {
              let B() = that
              ret self
          }
      }

      fun total[..Ts: std::ops::add::Add[Ts]](..xs: Ts) -> Void {
          let t = .. + xs
          std::mem::ops::drop(t)
      }

      fun f() -> Void {
          total(A(), B(), A())
      }
  )");

// A call fold over a pack ("j(xs)..") is a call per element, checked as a call with the element type: a callee generic
// over it ("j[T]") takes every element of a type pack. The fold is the tuple of what each returns.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_call_fold_over_a_type_pack, R"(
      fun size_of[T](t: T) -> S32 {
          std::mem::ops::drop(t)
          ret 1_s32
      }

      fun sizes[..Ts](..xs: Ts) -> Void {
          let r = size_of(xs)..
      }

      fun f() -> Void {
          sizes(1_s32, true, Str::from("a"))
      }
  )");

// A callee with no overload for the element type is rejected in the template, as any call of it would be.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_call_fold_over_a_type_pack_into_a_concrete_callee,
  SppFunctionCallNoValidSignaturesError, R"(
      fun size_of(t: S32) -> S32 {
          ret t
      }

      fun sizes[..Ts](..xs: Ts) -> Void {
          let r = size_of(xs)..
      }
  )");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_call_fold_over_a_pack_of_one_type_into_a_concrete_callee, R"(
      fun twice(t: S32) -> S32 {
          ret t + t
      }

      fun doubled(..xs: S32) -> Void {
          let r = twice(xs)..
      }

      fun f() -> Void {
          doubled(1_s32, 2_s32)
      }
  )");

// A spread in a call fold would give every element to each call.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_call_fold_over_a_spread_pack,
  SppSpreadInFoldError, R"(
      fun size_of[T](t: T) -> S32 {
          std::mem::ops::drop(t)
          ret 1_s32
      }

      fun sizes[..Ts](..xs: Ts) -> Void {
          let r = size_of(..xs)..
      }
  )");

// A pack folded with a tuple takes one element of each per call, so they must be as long.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_call_fold_over_a_pack_and_a_tuple_of_other_lengths,
  SppFoldLengthMismatchError, R"(
      fun add(a: S32, b: S32) -> S32 {
          ret a + b
      }

      fun sums(t: (S32, S32), ..xs: S32) -> Void {
          let r = add(t, xs)..
      }

      fun f() -> Void {
          sums((1_s32, 2_s32), 1_s32, 2_s32, 3_s32)
      }
  )");

// A call fold over a pack of linear elements moves each, so the pack is consumed.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_call_fold_consuming_a_pack, R"(
      fun eat[T](t: T) -> S32 {
          std::mem::ops::drop(t)
          ret 1_s32
      }

      fun take[..Ts](..xs: Ts) -> Void {
          let r = eat(xs)..
      }

      fun f() -> Void {
          take(Str::from("a"), Str::from("b"))
      }
  )");

// A borrowed pack is discarded the same way an owned one is consumed: "drop(..xs)" over "&Ts" picks the borrowed
// variadic "drop", and the values behind the borrows are still usable after.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_drop_of_a_borrowed_pack, R"(
      fun discard[..Ts](..xs: &Ts) -> Void {
          std::mem::ops::drop(..xs)
      }

      fun f() -> Void {
          let a = Str::from("a")
          let b = 1_s32
          discard(&a, &b)
          let c = a
          std::mem::ops::drop(c)
      }
  )");

// A borrowed pack of one type is discarded the same way.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_drop_of_a_borrowed_single_type_pack, R"(
      fun discard(..xs: &Str) -> Void {
          std::mem::ops::drop(..xs)
      }

      fun f() -> Void {
          let a = Str::from("a")
          let b = Str::from("b")
          discard(&a, &b)
          std::mem::ops::drop(a, b)
      }
  )");

// A call fold over a callee returning "Void" is "Void" itself, not a tuple of "Void"s: it is run for its effects, so it
// stands as a statement. Over a pack of one type, as "./project"'s "variadics.spp" has it.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_void_call_fold_as_a_statement, R"(
      fun take[T](a: Bool, ..b: T) -> Void {
          std::mem::ops::drop(a)
          std::mem::ops::drop(b)..
      }

      fun f() -> Void {
          take(true, Str::from("a"), Str::from("b"))
      }
  )");

// The same over a type pack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariadicPack,
  test_valid_void_call_fold_over_a_type_pack_as_a_statement, R"(
      fun take[..Ts](..xs: Ts) -> Void {
          std::mem::ops::drop(xs)..
      }

      fun f() -> Void {
          take(Str::from("a"), 1_s32)
      }
  )");

// A fold over a callee returning a value is still the tuple of the results, so discarding it is E95.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariadicPack,
  test_invalid_call_fold_of_values_discarded,
  SppDiscardedValueError, R"(
      fun one[T](t: T) -> S32 {
          std::mem::ops::drop(t)
          ret 1_s32
      }

      fun take[..Ts](..xs: Ts) -> Void {
          one(xs)..
      }

      fun f() -> Void {
          take(Str::from("a"), 1_s32)
      }
  )");
