#include "../test_macros.hpp"

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
