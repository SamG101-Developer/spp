#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_assign_through_a_mutable_borrow_parameter, R"(
    fun g(x: &mut S32) -> Void {
        x@ = 5
    }

    fun f() -> Void {
        let mut n = 0
        g(&mut n)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_compound_assign_through_a_mutable_borrow_parameter, R"(
    fun g(x: &mut S32) -> Void {
        x@ += 1
    }

    fun f() -> Void {
        let mut n = 0
        g(&mut n)
        g(&mut n)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_every_compound_operator_through_a_borrow, R"(
    fun g(x: &mut S32) -> Void {
        x@ += 1
        x@ -= 1
        x@ *= 2
        x@ /= 2
        x@ %= 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_read_through_a_shared_borrow, R"(
    fun g(x: &S32) -> S32 {
        ret x@ + 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_write_through_a_borrow_bound_by_a_case_pattern, R"(
    cls Point { !public x: S32, !public y: S32 }

    fun f() -> Void {
        let mut p = Point(x=1, y=2)
        case p is Point(&mut x, ..) { x@ += 1 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_write_through_a_yielded_mutable_borrow, R"(
    cls Holder { !public v: S32 }

    sup Holder {
        cor get_mut(&mut self) -> std::generator::GenOnce[&mut S32] {
            gen &mut self.v
        }
    }

    fun f() -> Void {
        let mut h = Holder(v=1)
        h.get_mut()@ += 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_valid_write_through_a_nested_field_borrow, R"(
    cls Inner { !public v: S32 }
    cls Outer { !public inner: Inner }

    fun g(x: &mut S32) -> Void {
        x@ = 9
    }

    fun f() -> Void {
        let mut o = Outer(inner=Inner(v=1))
        g(&mut o.inner.v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_invalid_write_through_a_shared_borrow,
  SppInvalidMutationError, R"(
    fun g(x: &S32) -> Void {
        x@ = 5
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentThroughBorrowAst,
  test_invalid_mutable_borrow_of_an_immutable_symbol,
  SppInvalidMutationError, R"(
    fun g(x: &mut S32) -> Void { }

    fun f() -> Void {
        let n = 0
        g(&mut n)
    }
)");
