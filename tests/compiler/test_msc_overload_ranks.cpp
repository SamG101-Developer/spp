#include "test_macros.hpp"

// Overloads are chosen by how well each argument matched its parameter ("MatchRank"), best first: the same type
// (exact), a type the argument widens into (widened), a type the argument chose by binding one of the callee's
// generics (bound), or reached through forwarding (forwarded). One overload beats another when no argument matched it
// worse and one matched it better; matching every argument alike, the one taking their count more tightly wins (fixed
// arity, then a variadic parameter of one element type, then one over a generic pack). Several left is ambiguous.
//
// Each passing test proves which overload won: the expected one returns "Bool", every other "S32", and the result is
// read into an untyped "let" before being used as a "Bool" (a typed "let" would choose by return type instead).

// --- One argument, each pair of ranks ----------------------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_beats_bound, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_beats_bound_declared_first, R"(
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }
    fun g(x: S32) -> Bool { ret true }

    fun f() -> Void {
        let r = g(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_beats_widened, R"(
    fun g(x: std::option::Some[S32]) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g(x: std::option::Opt[S32]) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32))
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_widened_chosen_when_exact_does_not_match, R"(
    fun g(x: std::option::Some[S32]) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }
    fun g(x: std::option::Opt[S32]) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }

    fun f() -> Void {
        let r = g(std::option::None)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_beats_forwarded, R"(
    fun g(x: &Vec[S32]) -> Bool { ret true }
    fun g(x: &std::view::View[S32]) -> S32 { ret 1_s32 }

    fun f(v: &Vec[S32]) -> Void {
        let r = g(v)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_forwarded_chosen_when_nothing_else_matches, R"(
    fun g(x: &std::view::View[S32]) -> Bool { ret true }
    fun g(x: &Str) -> S32 { ret 1_s32 }

    fun f(v: &Vec[S32]) -> Void {
        let r = g(v)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_widened_beats_bound, R"(
    fun g(x: std::option::Opt[S32]) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32))
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_bound_beats_forwarded, R"(
    fun g[T](x: &T) -> Bool { ret true }
    fun g(x: &std::view::View[S32]) -> S32 { ret 1_s32 }

    fun f(v: &Vec[S32]) -> Void {
        let r = g(v)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_beats_bound_inside_a_type, R"(
    fun g(x: std::option::Some[S32]) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g[T](x: std::option::Some[T]) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32))
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_borrow_beats_bound_borrow, R"(
    fun g(x: &Str) -> Bool { ret true }
    fun g[T](x: &T) -> S32 { ret 1_s32 }

    fun f(s: &Str) -> Void {
        let r = g(s)
        let b: Bool = r
    }
)");

// --- Several arguments: compared argument by argument -------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_better_on_one_argument_and_equal_on_the_rest, R"(
    fun g(x: S32, y: S32) -> Bool { ret true }
    fun g[T](x: S32, y: T) -> S32 {
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_better_on_every_argument, R"(
    fun g(x: S32, y: S32) -> Bool { ret true }
    fun g[T, U](x: T, y: U) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_one_generic_beats_two_generics, R"(
    fun g[T](x: S32, y: T) -> Bool {
        std::mem::ops::drop(y)
        ret true
    }
    fun g[T, U](x: T, y: U) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_widened_on_one_beats_bound_on_one, R"(
    fun g(x: std::option::Some[S32], y: std::option::Opt[S32]) -> Bool {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret true
    }
    fun g[T](x: std::option::Some[S32], y: T) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32), std::option::Some(val=2_s32))
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_keyword_arguments_ranked_by_parameter, R"(
    fun g(x: S32, y: S32) -> Bool { ret true }
    fun g[T](x: S32, y: T) -> S32 {
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(y=2_s32, x=1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_crossed_generics_are_ambiguous,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g[T](x: T, y: S32) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g[T](x: S32, y: T) -> S32 {
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_crossed_widening_is_ambiguous,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g(x: std::option::Some[S32], y: std::option::Opt[S32]) -> Bool {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret true
    }
    fun g(x: std::option::Opt[S32], y: std::option::Some[S32]) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32), std::option::Some(val=2_s32))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_crossed_exact_and_widened_is_ambiguous,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g[T](x: std::option::Opt[S32], y: T) -> Bool {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret true
    }
    fun g[T](x: T, y: std::option::Opt[S32]) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32), std::option::Some(val=2_s32))
    }
)");

// --- Arity: fixed, variadic of one element type, variadic over a pack ---------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_fixed_beats_variadic_with_one_argument, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g(..xs: S32) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_fixed_beats_variadic_of_the_same_length, R"(
    fun g(x: S32, y: S32, z: S32) -> Bool { ret true }
    fun g(..xs: S32) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_variadic_chosen_at_another_length, R"(
    fun g(x: S32, y: S32, z: S32) -> S32 { ret 1_s32 }
    fun g(..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32, 4_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_concrete_variadic_beats_generic_variadic, R"(
    fun g(..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g[T](..xs: T) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_generic_variadic_beats_pack_variadic, R"(
    fun g[T](..xs: T) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g[..Ts](..xs: Ts) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_pack_variadic_chosen_for_mixed_types, R"(
    fun g[T](..xs: T) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }
    fun g[..Ts](..xs: Ts) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }

    fun f() -> Void {
        let r = g(1_s32, true, 3_u64)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_concrete_variadic_beats_pack_variadic, R"(
    fun g(..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g[..Ts](..xs: Ts) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_fixed_beats_every_variadic_of_the_same_length, R"(
    fun g(x: S32, y: S32, z: S32) -> Bool { ret true }
    fun g(..xs: S32) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }
    fun g[T](..xs: T) -> U32 {
        std::mem::ops::drop(xs)
        ret 2_u32
    }
    fun g[..Ts](..xs: Ts) -> U64 {
        std::mem::ops::drop(xs)
        ret 3_u64
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_concrete_variadic_beats_both_generic_variadics, R"(
    fun g(..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g[T](..xs: T) -> U32 {
        std::mem::ops::drop(xs)
        ret 2_u32
    }
    fun g[..Ts](..xs: Ts) -> U64 {
        std::mem::ops::drop(xs)
        ret 3_u64
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_fixed_generic_beats_generic_variadic_of_the_same_length, R"(
    fun g[T](x: T, y: T, z: T) -> Bool {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        std::mem::ops::drop(z)
        ret true
    }
    fun g[T](..xs: T) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_variadic_beats_fixed_generic_of_the_same_length, R"(
    fun g(..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g[T](x: T, y: T, z: T) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        std::mem::ops::drop(z)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_fixed_prefix_beats_variadic_prefix, R"(
    fun g(x: S32, ..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g[T](x: T, ..xs: S32) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(xs)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

// A variadic over one generic element type and one over a generic pack are different overloads, whatever they return:
// a call of mixed types reaches only the pack ("IdTermsOf" pairs a pack only with a pack).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_generic_and_pack_variadics_with_one_return_type_coexist, R"(
    fun g[T](..xs: T) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }
    fun g[..Ts](..xs: Ts) -> S32 {
        std::mem::ops::drop(xs)
        ret 2_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let s = g(1_s32, true)
    }
)");

// --- Methods: the receiver ranks alike, the rest decide -----------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_method_exact_beats_bound, R"(
    cls A { }
    sup A {
        !public fun m(&self, x: S32) -> Bool { ret true }
        !public fun m[T](&self, x: T) -> S32 {
            std::mem::ops::drop(x)
            ret 1_s32
        }
    }

    fun f(a: &A) -> Void {
        let r = a.m(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_method_static_form_exact_beats_bound, R"(
    cls A { }
    sup A {
        !public fun m(&self, x: S32) -> Bool { ret true }
        !public fun m[T](&self, x: T) -> S32 {
            std::mem::ops::drop(x)
            ret 1_s32
        }
    }

    fun f(a: &A) -> Void {
        let r = A::m(a, 1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_method_on_a_generic_type_exact_beats_bound, R"(
    cls Box[T] { !public v: T }
    sup [T] Box[T] {
        !public fun m(&self, x: S32) -> Bool { ret true }
        !public fun m[U](&self, x: U) -> S32 {
            std::mem::ops::drop(x)
            ret 1_s32
        }
    }

    fun f(a: &Box[Bool]) -> Void {
        let r = a.m(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_type_parameter_fixed_by_the_receiver_ranks_exact, R"(
    cls Box[T] { !public v: T }
    sup [T] Box[T] {
        !public fun m(&self, x: T) -> Bool {
            std::mem::ops::drop(x)
            ret true
        }
        !public fun m[U](&self, x: U) -> S32 {
            std::mem::ops::drop(x)
            ret 1_s32
        }
    }

    fun f(a: &Box[S32]) -> Void {
        let r = a.m(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_method_crossed_generics_are_ambiguous,
  SppFunctionCallOverloadAmbiguousError, R"(
    cls A { }
    sup A {
        !public fun m[T](&self, x: T, y: S32) -> Bool {
            std::mem::ops::drop(x)
            ret true
        }
        !public fun m[T](&self, x: S32, y: T) -> S32 {
            std::mem::ops::drop(y)
            ret 1_s32
        }
    }

    fun f(a: &A) -> Void {
        let r = a.m(1_s32, 2_s32)
    }
)");

// --- Defaults: a parameter the call left out takes no part --------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_default_does_not_rank, R"(
    fun g(x: S32, y: S32 = 5_s32) -> Bool { ret true }
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_bound_with_default_loses_to_exact, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g[T](x: T, y: Bool = false) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32)
        let b: Bool = r
    }
)");

// --- Explicit generic arguments: chosen by the call, not by the argument ------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_explicit_generic_argument_ranks_exact, R"(
    fun g[T](x: T, y: S32) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g[T, U](x: T, y: U) -> S32 {
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g[T=S32](1_s32, 2_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_explicit_generic_argument_picks_the_generic, R"(
    fun g(x: S32) -> S32 { ret 1_s32 }
    fun g[T](x: T) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }

    fun f() -> Void {
        let r = g[S32](1_s32)
        let b: Bool = r
    }
)");

// --- The return type the context asks for narrows first ------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_return_type_narrows_before_rank, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r: S32 = g(1_s32)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_rank_decides_when_return_type_does_not, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g[T](x: T) -> Bool {
        std::mem::ops::drop(x)
        ret false
    }

    fun f() -> Void {
        let r: Bool = g(1_s32)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_rank_decides_an_argument_that_is_a_call, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }
    fun h(b: Bool) -> Bool { ret b }

    fun f() -> Void {
        let r = h(g(1_s32))
        let b: Bool = r
    }
)");

// --- Ties stay ambiguous ------------------------------------------------------------------------------------------

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_two_bound_overloads_differing_only_in_return_type,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g[T](x: T) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g[T](x: T) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_two_exact_overloads_differing_only_in_return_type,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g(x: S32) -> Bool { ret true }
    fun g(x: S32) -> S32 { ret 1_s32 }

    fun f() -> Void {
        let r = g(1_s32)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_never_widens_alike_into_both,
  SppFunctionCallOverloadAmbiguousError, R"(
    use std::abort::abort
    fun g(x: S32) -> Void { }
    fun g(x: Bool) -> Void { }

    fun f() -> Void {
        g(abort())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_widened_into_two_variants_alike,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g(x: std::option::Opt[S32]) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g(x: std::option::Some[S32] or Bool) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloadRanks,
  test_invalid_two_variadics_of_one_kind_differing_in_return_type,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g(..xs: S32) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }
    fun g(..ys: S32) -> S32 {
        std::mem::ops::drop(ys)
        ret 1_s32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32)
    }
)");

// --- More combinations ----------------------------------------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_exact_beats_widened_and_bound_together, R"(
    fun g(x: std::option::Some[S32]) -> Bool {
        std::mem::ops::drop(x)
        ret true
    }
    fun g(x: std::option::Opt[S32]) -> S32 {
        std::mem::ops::drop(x)
        ret 1_s32
    }
    fun g[T](x: T) -> U32 {
        std::mem::ops::drop(x)
        ret 2_u32
    }

    fun f() -> Void {
        let r = g(std::option::Some(val=1_s32))
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_bound_beats_generic_forwarded, R"(
    fun g[T](x: &Vec[T]) -> Bool { ret true }
    fun g[T](x: &std::view::View[T]) -> S32 { ret 1_s32 }

    fun f(v: &Vec[S32]) -> Void {
        let r = g(v)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_three_arguments_dominance_chain, R"(
    fun g(x: S32, y: S32, z: S32) -> Bool { ret true }
    fun g[T](x: S32, y: S32, z: T) -> S32 {
        std::mem::ops::drop(z)
        ret 1_s32
    }
    fun g[T, U](x: S32, y: T, z: U) -> U32 {
        std::mem::ops::drop(y)
        std::mem::ops::drop(z)
        ret 2_u32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_middle_of_a_dominance_chain_when_the_best_does_not_match, R"(
    fun g(x: S32, y: S32, z: Bool) -> U64 { ret 0_u64 }
    fun g[T](x: S32, y: S32, z: T) -> Bool {
        std::mem::ops::drop(z)
        ret true
    }
    fun g[T, U](x: S32, y: T, z: U) -> U32 {
        std::mem::ops::drop(y)
        std::mem::ops::drop(z)
        ret 2_u32
    }

    fun f() -> Void {
        let r = g(1_s32, 2_s32, 3_s32)
        let b: Bool = r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_method_through_a_forwarding_receiver_ranks_its_arguments, R"(
    fun f(v: &Vec[S32]) -> Void {
        let r = v.len()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloadRanks,
  test_valid_explicit_element_type_picks_the_generic_variadic, R"(
    fun g(..xs: S32) -> S32 {
        std::mem::ops::drop(xs)
        ret 1_s32
    }
    fun g[T](..xs: T) -> Bool {
        std::mem::ops::drop(xs)
        ret true
    }

    fun f() -> Void {
        let r = g[S32](1_s32, 2_s32)
        let b: Bool = r
    }
)");
