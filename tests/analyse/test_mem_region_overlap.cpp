#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_two_shared_borrows_of_one_value, R"(
    fun g(a: &Str, b: &Str) -> Void { }

    fun f() -> Void {
        let s = Str::from("x")
        g(&s, &s)
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_borrow_and_move_of_distinct_values, R"(
    fun g(a: &mut Str, b: Str) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        let mut a = Str::from("x")
        let b = Str::from("y")
        g(&mut a, b)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_names_sharing_a_spelling_prefix, R"(
    fun g(a: &mut Str, b: Str) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        let mut s = Str::from("x")
        let second = Str::from("y")
        g(&mut s, second)
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_mutable_borrow_of_a_name_prefixing_another, R"(
    fun g(a: &mut Str, b: &Str) -> Void { }

    fun f() -> Void {
        let mut a = Str::from("x")
        let ab = Str::from("y")
        g(&mut a, &ab)
        std::mem::ops::drop(a)
        std::mem::ops::drop(ab)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_borrows_of_sibling_fields, R"(
    cls P {
        !public a: Str
        !public b: Str
    }

    fun g(x: &mut Str, y: &Str) -> Void { }

    fun f() -> Void {
        let mut p = P(a=Str::from("x"), b=Str::from("y"))
        g(&mut p.a, &p.b)
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_two_mutable_borrows_of_one_value,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut Str, b: &mut Str) -> Void { }

    fun f() -> Void {
        let mut s = Str::from("x")
        g(&mut s, &mut s)
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_mutable_and_shared_borrow_of_one_value,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut Str, b: &Str) -> Void { }

    fun f() -> Void {
        let mut s = Str::from("x")
        g(&mut s, &s)
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_borrow_and_move_of_one_value,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &Str, b: Str) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        let s = Str::from("x")
        g(&s, s)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_whole_value_and_one_of_its_fields,
  SppMemoryOverlapUsageError, R"(
    cls P {
        !public a: Str
        !public b: Str
    }

    fun g(x: &mut P, y: &Str) -> Void { }

    fun f() -> Void {
        let mut p = P(a=Str::from("x"), b=Str::from("y"))
        g(&mut p, &p.a)
        std::mem::ops::drop(p)
    }
)");

// Two subscripts of one value are one place whenever their indices agree, and nothing here can decide whether they
// do. The overlap check used to compare the two expressions as rendered text, which made "v[mut i]" and "v[mut j]"
// two different places on the strength of two different spellings - and let a pair of mutable borrows of one element
// through the law of exclusivity. Comparing the access paths step by step answers it: a subscript contributes no
// named step, so the two paths are the same path.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_two_mutable_subscripts_of_one_value,
  SppMemoryOverlapUsageError, R"(
    fun f() -> Void {
        let mut v = [1, 2, 3]
        let i = 0_uz
        let j = 1_uz
        std::mem::ops::swap(v[mut i], v[mut j])
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_mutable_subscript_and_whole_value,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut std::array::Arr[std::number::S32, 3_uz], b: &mut std::number::S32) -> Void { }

    fun f() -> Void {
        let mut v = [1, 2, 3]
        let i = 0_uz
        g(&mut v, v[mut i])
    }
)");

// The other direction: a subscript of one value against a subscript of another is two places whatever the indices
// are, because the two paths differ in their first named step.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_mutable_subscripts_of_distinct_values, R"(
    fun f() -> Void {
        let mut u = [1, 2, 3]
        let mut v = [4, 5, 6]
        let i = 0_uz
        std::mem::ops::swap(u[mut i], v[mut i])
    }
)");

// A subscript or a slice reaches a part of its value chosen at run time, so it overlaps anything else taken from the
// same value, whatever method it lowers to ("index_mut", "index_ref", "slice_*"). These pin that a subscript or a
// slice names its whole value.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_mutable_subscript_and_shared_subscript,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut S32, b: &S32) -> Void { }

    fun f() -> Void {
        let mut v = [1, 2, 3]
        let i = 0_uz
        let j = 1_uz
        g(v[mut i], v[j])
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_mutable_subscript_and_shared_slice,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut S32, b: &View[S32]) -> Void { }

    fun f(mut v: Vec[S32]) -> Void {
        let i = 0_uz
        g(v[mut i], v[1_uz to 2_uz])
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_mutable_slice_and_shared_subscript,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut View[S32], b: &S32) -> Void { }

    fun f(mut v: Vec[S32]) -> Void {
        let i = 0_uz
        g(v[mut 1_uz to 2_uz], v[i])
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_mutable_slice_and_shared_slice,
  SppMemoryOverlapUsageError, R"(
    fun g(a: &mut View[S32], b: &View[S32]) -> Void { }

    fun f(mut v: Vec[S32]) -> Void {
        g(v[mut 0_uz to 1_uz], v[1_uz to 2_uz])
        std::mem::ops::drop(v)
    }
)");

// A borrow yielded by a method call can point anywhere inside its receiver: the call is opaque. Comparing the method
// by name made "p.a_mut()" and "p.b" two places, and "p.a_mut()" and "p.b_ref()" too, though "a_mut" is free to yield
// "&mut self.b".

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_yielded_mutable_borrow_and_a_field_of_its_receiver,
  SppMemoryOverlapUsageError, R"(
    cls P {
        !public a: S32
        !public b: S32
    }

    sup P {
        !public cor a_mut(&mut self) -> std::generator::GenOnce[&mut S32] {
            gen &mut self.a
        }
    }

    fun g(x: &mut S32, y: &S32) -> Void { }

    fun f() -> Void {
        let mut p = P(a=1, b=2)
        g(p.a_mut(), &p.b)
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemRegionOverlap,
  test_invalid_yielded_mutable_borrow_and_another_yielded_borrow,
  SppMemoryOverlapUsageError, R"(
    cls P {
        !public a: S32
        !public b: S32
    }

    sup P {
        !public cor a_mut(&mut self) -> std::generator::GenOnce[&mut S32] {
            gen &mut self.a
        }

        !public cor b_ref(&self) -> std::generator::GenOnce[&S32] {
            gen &self.b
        }
    }

    fun g(x: &mut S32, y: &S32) -> Void { }

    fun f() -> Void {
        let mut p = P(a=1, b=2)
        g(p.a_mut(), p.b_ref())
        std::mem::ops::drop(p)
    }
)");

// A field still separates: a call under one field cannot reach a sibling field.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemRegionOverlap,
  test_valid_yielded_mutable_borrow_and_a_sibling_field, R"(
    cls Q {
        !public a: S32
    }

    sup Q {
        !public cor a_mut(&mut self) -> std::generator::GenOnce[&mut S32] {
            gen &mut self.a
        }
    }

    cls P {
        !public q: Q
        !public b: S32
    }

    fun g(x: &mut S32, y: &S32) -> Void { }

    fun f() -> Void {
        let mut p = P(q=Q(a=1), b=2)
        g(p.q.a_mut(), &p.b)
        std::mem::ops::drop(p)
    }
)");
