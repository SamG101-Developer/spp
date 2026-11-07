#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_borrow_as_a_parameter, R"(
    fun g(a: &Str, b: &mut Str) -> Void { }

    fun f() -> Void {
        let mut s = Str::from("x")
        let mut t = Str::from("y")
        g(&s, &mut t)
        std::mem::ops::drop(s)
        std::mem::ops::drop(t)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_borrow_yielded_from_a_coroutine, R"(
    cls Holder { !public val: S32 }

    sup Holder {
        !public cor peek(&self) -> std::generator::GenOnce[&S32] {
            gen &self.val
        }
    }

    fun f() -> Void {
        let h = Holder(val=1)
        let a = h.peek()@
        std::mem::ops::drop(h)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_a_function_return_type,
  SppSecondClassBorrowViolationError, R"(
    fun g(a: &Str) -> &Str {
        ret a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_mutable_borrow_as_a_function_return_type,
  SppSecondClassBorrowViolationError, R"(
    fun g(a: &mut Str) -> &mut Str {
        ret a
    }
)");

// Taking every non-copyable part off a value consumes it: nothing owned is left, just as after destructuring it.
// Case patterns rely on this, as they record the parts they bind rather than consuming the subject.
// FIXED (the test expected E94; moving the only field out is equivalent to the destructure below)
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_partial_move_of_every_part_out_of_a_consumed_self, R"(
    cls Holder { !public val: Str }

    sup Holder {
        fun take(self) -> Str {
            ret self.val
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_partial_move_leaving_a_part_of_a_consumed_self,
  SppLinearValueNotConsumedError, R"(
    cls Holder { !public a: Str
    !public b: Str }

    sup Holder {
        fun take(self) -> Str {
            ret self.a
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_partial_move_of_only_a_copyable_part_of_a_consumed_self,
  SppLinearValueNotConsumedError, R"(
    cls Holder { !public n: S32
    !public b: Str }

    sup Holder {
        fun take(self) -> S32 {
            ret self.n
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_partial_move_out_of_a_consumed_self_with_a_drop,
  SppPartialMoveOfDestructibleValueError, R"(
    cls Holder { !public val: Str }

    sup Holder ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Holder(val) = self
            std::mem::ops::drop(val)
        }
    }

    sup Holder {
        fun take(self) -> Str {
            ret self.val
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_destructure_instead_of_partial_move, R"(
    cls Holder { !public val: Str }

    sup Holder {
        fun take(self) -> Str {
            let Holder(val) = self
            ret val
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_a_comp_generic_parameter,
  SppSecondClassBorrowViolationError, R"(
    cls X[cmp n: &S32] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_owned_comp_generic_parameter, R"(
    cls X[cmp n: S32] { }

    fun f() -> Void {
        let a = X[1]()
        std::mem::ops::drop(a)
    }
)");

// A borrow cannot be a generic argument, as a value of the type would then store it: a borrow is never stored in a
// value. A generator's arguments (its yield) and a function type's (its signature) name a borrow without storing one,
// and are the exceptions, found through the type's sups.

// The hole this closes: "Vec" holds its "T" through raw memory, not a field, so nothing knew "v" held a loan on "a",
// and "v.len()" read a dangling borrow.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_a_container_element_outliving_its_target,
  SppSecondClassBorrowViolationError, R"(
    fun f() -> Void {
        let mut v = Vec[&Str]::new()
        {
            let a = Str::from("x")
            v.append(&a)
            std::mem::ops::drop(a)
        }
        let n = v.len()
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_a_container_element_returned,
  SppSecondClassBorrowViolationError, R"(
    fun f(v: Vec[&Str]) -> Vec[&Str] {
        ret v
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_a_container_element_in_a_field,
  SppSecondClassBorrowViolationError, R"(
    cls Holder {
        !public v: Vec[&Str]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_nested_inside_a_generic_argument,
  SppSecondClassBorrowViolationError, R"(
    fun f(v: Vec[Vec[&Str]]) -> Void {
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_in_a_variant_as_a_generic_argument,
  SppSecondClassBorrowViolationError, R"(
    fun f(v: Vec[&Str or None]) -> Void {
        std::mem::ops::drop(v)
    }
)");

// The instance is made, and its field checked, before the generic argument check runs: still reported where the
// borrow is written, with the field as context ("InstantiatedFrom").
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_a_generic_argument_of_a_type_with_a_field_of_it,
  SppSecondClassBorrowViolationError, R"(
    fun f(v: Opt[&Str]) -> Void {
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_borrow_as_an_explicit_function_generic_argument,
  SppSecondClassBorrowViolationError, R"(
    fun id[T](x: T) -> Void {
        std::mem::ops::drop(x)
    }

    fun f() -> Void {
        let a = Str::from("x")
        id[&Str](&a)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_borrow_as_a_function_types_parameter, R"(
    fun sort_with[F: FunRef[(&Str, &Str), Bool]](a: &Str, b: &Str, less: F) -> Bool {
        ret less(a, b)
    }

    fun never(x: &Str, y: &Str) -> Bool {
        ret false
    }

    fun f() -> Void {
        let a = Str::from("x")
        let b = Str::from("y")
        let less = sort_with(&a, &b, never)
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_borrow_yielded_by_a_users_generator_class, R"(
    cls Once[T] { }

    sup [T] Once[T] ext std::generator::GenOnce[T] { }

    cls Holder { !public val: S32 }

    sup Holder {
        !public cor peek(&self) -> Once[&S32] {
            gen &self.val
        }
    }

    fun f() -> Void {
        let h = Holder(val=1)
        let a = h.peek()@
        std::mem::ops::drop(h)
    }
)");

// Guards the exemption for a class superimposing "GenOnce", as the std's indexing does ("Indexed[&T]").
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_borrow_yielded_by_an_indexed_generator, R"(
    cls Holder { !public val: S32 }

    sup Holder {
        !public cor peek(&self) -> std::iterator::Indexed[&S32] {
            gen &self.val
        }
    }

    fun f() -> Void {
        let h = Holder(val=1)
        let a = h.peek()@
        std::mem::ops::drop(h)
    }
)");

// Yielding a borrow into a variant holding it ("&S32 or None") wraps it as that member (open-bugs.md #49: the lowered
// "GenOnce" returned the bare pointer where the variant was due, an ICE).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemSecondClassBorrows,
  test_valid_borrow_or_none_yielded_by_an_indexed_generator, R"(
    cls Holder { !public val: S32 }

    sup Holder {
        !public cor maybe(&self) -> std::iterator::Indexed[&S32 or None] {
            gen &self.val
        }
    }

    fun f() -> Void {
        let h = Holder(val=1)
        let g = h.maybe()
        std::mem::ops::drop(g)
        std::mem::ops::drop(h)
    }
)");
