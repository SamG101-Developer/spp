#include "../test_macros.hpp"

// Every route to two ways into one place reaching a call, where one is "&mut" (M7.3): each must be rejected for "noalias"
// on "&mut" parameters to be sound.

#define ALIAS_ROUTES_PRELUDE R"(
    cls A {
        !public s: Str
        !public n: S32
    }

    sup A {
        !public fun f(&mut self, x: &Str) -> Void { }
        !public fun g(&mut self, x: &mut S32) -> Void { }
    }

    fun two(a: &mut A, b: &mut S32) -> Void { }
    fun twice(a: &mut A, b: &mut A) -> Void { }
    fun take_c(c: FunMut[(), Void], y: &mut S32) -> Void { std::mem::ops::drop(c) }
    fun read(y: &S32) -> Void { }
    fun write(y: &mut S32) -> Void { }
    cor counter(y: &mut S32) -> Gen[S32] {
        gen 1
        gen 2
    }
)"

// "&mut self" and "&a.s" in one call.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_receiver_mut_self_beside_a_ref_of_its_part,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t(mut a: A) -> Void {
        a.f(&a.s)
        std::mem::ops::drop(a)
    }
)");

// "&mut self" and "&mut a.n" in one call.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_receiver_mut_self_beside_a_mut_of_its_part,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t(mut a: A) -> Void {
        a.g(&mut a.n)
        std::mem::ops::drop(a)
    }
)");

// A "&mut" parameter as the receiver, and a "&mut" of its part.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_borrowed_receiver_beside_a_mut_of_its_part,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t(p: &mut A) -> Void {
        p.g(&mut p.n)
    }
)");

// A "&mut" parameter passed on (moved), then its part borrowed.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_mut_parameter_passed_on_beside_its_part,
  SppUninitializedMemoryUseError, ALIAS_ROUTES_PRELUDE R"(
    fun t(p: &mut A) -> Void {
        two(p, &mut p.n)
    }
)");

// The same, the other way round.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_part_borrowed_beside_its_mut_parameter,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun two_rev(b: &mut S32, a: &mut A) -> Void { }
    fun t(p: &mut A) -> Void {
        two_rev(&mut p.n, p)
    }
)");

// One "&mut" parameter passed twice.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_mut_parameter_passed_twice,
  SppUninitializedMemoryUseError, ALIAS_ROUTES_PRELUDE R"(
    fun t(p: &mut A) -> Void {
        twice(p, p)
    }
)");

// A closure holding "&mut x" while "x" is borrowed again.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_mut_borrow_while_a_closure_holds_one,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t() -> Void {
        let mut x = 1_s32
        let c = (caps &mut x) { x@ = 2_s32 }
        write(&mut x)
        c()
    }
)");

// A closure argument capturing "&mut x", beside "&mut x".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_closure_argument_capturing_what_another_argument_borrows,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t() -> Void {
        let mut x = 1_s32
        take_c((caps &mut x) { x@ = 2_s32 }, &mut x)
    }
)");

// The same, the other way round.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_argument_borrowing_what_a_closure_argument_captures,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t() -> Void {
        let mut x = 1_s32
        take_c2(&mut x, (caps &mut x) { x@ = 2_s32 })
    }
    fun take_c2(y: &mut S32, c: FunMut[(), Void]) -> Void { std::mem::ops::drop(c) }
)");

// A generator holding "&mut x" across resumes, "x" borrowed between.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_borrow_across_a_resume_of_a_generator_holding_mut,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t() -> Void {
        let mut x = 1_s32
        let mut g = counter(&mut x)
        let a = g.res()
        read(&x)
        let b = g.res()
    }
)");

// A future holding "&mut x" before it is awaited.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_borrow_while_a_future_holds_mut,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun m(a: &mut S32) -> S32 { ret 1 }
    fun t() -> Void {
        let mut x = 1_s32
        let f = async m(&mut x)
        read(&x)
        let r = f.await
    }
)");

// A pattern binding "&mut n" passed beside "&mut a".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_pattern_borrow_beside_its_subject,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t(mut a: A) -> Void {
        case a of {
            is A(&mut n, ..) { two(&mut a, n) }
        }
        std::mem::ops::drop(a)
    }
)");

// "v[mut 0]" while "v.iter_mut()" is being iterated.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_index_borrow_while_iterating_mut,
  SppMemoryOverlapUsageError, ALIAS_ROUTES_PRELUDE R"(
    fun t() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        loop mut e in v.iter_mut() {
            write(&mut v[mut 0_uz])
        }
        std::mem::ops::drop(v)
    }
)");

// A binding that is itself a borrow passes it on when given away; the loan it holds stays until it ends.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemAliasRoutes,
  test_valid_index_borrow_binding_passed_on, ALIAS_ROUTES_PRELUDE R"(
    fun t() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        {
            let a = v[mut 0_uz]
            write(a)
        }
        std::mem::ops::drop(v)
    }
)");

// A pattern binding passed on alone is one way to its place.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemAliasRoutes,
  test_valid_pattern_borrow_passed_on_alone, ALIAS_ROUTES_PRELUDE R"(
    fun t(mut a: A) -> Void {
        case a of {
            is A(&mut n, ..) { write(n) }
        }
        std::mem::ops::drop(a)
    }
)");

// M7.4 acceptance: the swap that "Arr::swap" / "Vec::swap" must not be written as, and the same through bindings.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_swap_of_two_index_borrows_of_an_array,
  SppMemoryOverlapUsageError, R"(
    fun bad_swap(a: &mut std::array::Arr[S32, 4_uz], i: USize, j: USize) -> Void {
        std::mem::ops::swap(a[mut i], a[mut j])
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_swap_of_two_index_borrows_of_a_vector,
  SppMemoryOverlapUsageError, R"(
    fun bad_swap(v: &mut std::vector::Vec[S32], i: USize, j: USize) -> Void {
        std::mem::ops::swap(v[mut i], v[mut j])
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemAliasRoutes,
  test_invalid_swap_of_two_index_borrows_held_by_bindings,
  SppMemoryOverlapUsageError, R"(
    fun t() -> Void {
        let mut v = std::vector::Vec[S32]::from(&[1, 2])
        {
            let a = v[mut 0_uz]
            let b = v[mut 1_uz]
            std::mem::ops::swap(a, b)
        }
        std::mem::ops::drop(v)
    }
)");
