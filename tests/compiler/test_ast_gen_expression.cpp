#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_expression_in_subroutine,
    SppFunctionSubroutineContainsGenExpressionError, R"(
    fun foo() -> Void {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mov_vs_mut,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_ref_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[&S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_mov,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[&mut S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_convention_mismatch_mut_vs_ref,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[&mut S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_type_mismatch,
    SppYieldedTypeMismatchError, R"(
    cor foo() -> Gen[S32] {
        gen false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut_coercion,
    R"(
    cor foo() -> Gen[&S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mov,
    R"(
    cor foo() -> Gen[S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_ref,
    R"(
    cor foo() -> Gen[&S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_convention_mut,
    R"(
    cor foo() -> Gen[&mut S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_empty_gen_void,
    R"(
    cor foo() -> Gen[Void] {
        gen
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_gen_mut_borrow_of_mutable_symbol,
    R"(
    cor foo(mut x: S32) -> Gen[&mut S32] {
        gen &mut x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_mut_borrow_of_immutable_symbol,
    SppInvalidMutationError, R"(
    cor foo(x: S32) -> Gen[&mut S32] {
        gen &mut x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_gen_moved_symbol_reused,
    SppUninitializedMemoryUseError, R"(
    cor foo(x: Str) -> Gen[Str] {
        gen x
        gen x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  GenExpressionAst,
  test_valid_gen_named_function_into_a_function_yield_type, R"(
    fun add_one(x: S32) -> S32 { ret x + 1 }

    cor g() -> Gen[std::function::FunRef[(S32,), S32]] {
        gen add_one
    }

    fun f() -> Void {
        loop h in g() {
            let r = h(1)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  GenExpressionAst,
  test_valid_gen_member_into_a_variant_yield_type, R"(
    cor g() -> Gen[std::option::Opt[S32]] {
        gen std::option::Some[S32](val=1)
        gen std::option::None
    }
)");

// A yielded borrow that already had the yield type was forwarded anyway ("&Str" became "&StrView"), and then
// mismatched.
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_gen_borrow_of_a_str_as_a_str_borrow, R"(
    cor f() -> Gen[&Str] {
        let elem = Str::from("x")
        gen &elem
        std::mem::ops::drop(elem)
    }
)");

// A generator destroyed at a "gen" drops what its frame still owns there, innermost scope and newest first, after the
// "defer"s destroying it runs ("OwnedAtSuspend"); "tests/codegen/check-drops.sh" checks what runs.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_owned_values_at_a_gen, R"(
    cls Noisy {
        !public n: S32
    }

    sup Noisy ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Noisy(..) = self
        }
    }

    cor g(x: Noisy) -> Gen[S32] {
        let a = Noisy(n=1)
        {
            let b = Noisy(n=2)
            gen 1
            std::mem::ops::drop(b)
        }
        let c = Noisy(n=3)
        gen 2
        std::mem::ops::drop(a)
        std::mem::ops::drop(c)
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_owned_values_at_a_gen_in_a_loop_and_a_generic, R"(
    cor g[T](x: T) -> Gen[U64] {
        let mut i = 0_u64
        loop i < 3_u64 {
            let s = Str::from(i)
            gen i
            std::mem::ops::drop(s)
            i += 1_u64
        }
        std::mem::ops::drop(x)
    }

    fun f() -> Void {
        let mut c = g(Str::from("a"))
        let r = c.res()
        std::mem::ops::drop(c)
    }
)");

// The "defer"s run where a generator is destroyed, so one taking a value already moved there would take it twice.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_defer_consuming_a_value_moved_before_a_gen,
    SppDeferConsumesMovedValueError, R"(
    cor g(x: Str) -> Gen[U64] {
        defer std::mem::ops::drop(x)
        let y = x
        gen 1_u64
        std::mem::ops::drop(y)
    }
)");

// A borrow a loop's generator yields is handed on by the "gen", and the loop's "defer" drops the generator: the frame
// being destroyed ends every borrow, so that drop is not a move of borrowed memory.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_gen_of_a_borrow_yielded_by_a_loop_over_a_generator, R"(
    cor g(v: &mut std::vector::Vec[S32]) -> Gen[&mut S32] {
        loop x in v.iter_mut() {
            gen x
        }
    }
)");

// A value partly moved out at a "gen" loses only what is left of it when the frame is destroyed there.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenExpressionAst,
    test_valid_partly_moved_value_at_a_gen, R"(
    cls Pair2 {
        !public a: Str
        !public b: Str
    }

    cor g() -> Gen[U64] {
        let p = Pair2(a=Str::from("a"), b=Str::from("b"))
        std::mem::ops::drop(p.a)
        gen 1_u64
        std::mem::ops::drop(p.b)
    }
)");

// A frame destroyed at a "gen" destroys a value missing a part there, which a "drop" of its own cannot run on: putting
// the part back after the "gen" does not reach that destruction.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenExpressionAst,
    test_invalid_destructible_value_partly_moved_at_a_gen,
    SppPartialMoveOfDestructibleValueError, R"(
    cls Holder {
        !public a: Str
        !public b: Str
    }

    sup Holder ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Holder(a, b) = self
            std::mem::ops::drop(a)
            std::mem::ops::drop(b)
        }
    }

    cor g() -> Gen[U64] {
        let mut h = Holder(a=Str::from("a"), b=Str::from("b"))
        std::mem::ops::drop(h.a)
        gen 1_u64
        h.a = Str::from("c")
        std::mem::ops::drop(h)
    }
)");
