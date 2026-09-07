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

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemSecondClassBorrows,
  test_invalid_partial_move_out_of_a_consumed_self,
  SppLinearValueNotConsumedError, R"(
    cls Holder { !public val: Str }

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
