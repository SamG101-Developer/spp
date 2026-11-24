#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_array_literal_as_statement, R"(
    fun f() -> Void {
        [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_identifier_as_statement, R"(
    fun f() -> Void {
        let x = 123
        x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_call_returning_value_as_statement, R"(
    fun g() -> S32 { ret 1 }

    fun f() -> Void {
        g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropDiscardedValue,
    test_valid_void_call_as_statement, R"(
    fun g() -> Void { }

    fun f() -> Void {
        g()
    }
)");

// The discard follows the "case" inwards: it is the "1" and the "2" that the branches end on that are discarded, not
// the "case" itself, because that is where a value nothing takes is actually produced.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_case_as_statement_reports_branch_finals, R"(
    fun f() -> Void {
        let c = true
        case c { 1 } else { 2 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropDiscardedValue,
    test_valid_case_as_statement_with_void_branches, R"(
    fun g() -> Void { }

    fun f() -> Void {
        let c = true
        case c { g() } else { g() }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropDiscardedValue,
    test_valid_case_bound_by_let, R"(
    fun f() -> Void {
        let c = true
        let x = case c { 1 } else { 2 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_block_as_statement, R"(
    fun f() -> Void {
        { 1 }
    }
)");

// A loop takes no value from its body: the final statement of an iteration is discarded like any other.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_value_as_a_loop_bodys_final_statement, R"(
    fun f(c: Bool) -> Void {
        loop c {
            1_s32
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_linear_value_as_a_loop_bodys_final_statement, R"(
    cls OwHandleD { }

    fun f(c: Bool) -> Void {
        loop c {
            OwHandleD()
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropDiscardedValue,
    test_valid_void_as_a_loop_bodys_final_statement, R"(
    fun f() -> Void {
        let mut i = 0_s32
        loop i < 3_s32 {
            i += 1_s32
        }
        loop x in [1_s32, 2_s32].iter_mov() {
            i += x
        }
    }
)");

// A "case" whose value nothing takes needs no branch type agreement: a value in one branch is discarded there, and
// dropped at the end of its statement, not a mismatch with the others (E1).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_value_in_one_branch_of_a_statement_case, R"(
    cls OwHandleD { }

    fun f(c: Bool) -> Void {
        case c { OwHandleD() }
        else { }
        let x = 1_s32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDiscardedValue,
  test_valid_value_as_an_iterable_loop_bodys_final_statement, R"(
    cls OwHandleD { }

    fun f() -> Void {
        loop x in [1_s32, 2_s32].iter_mov() {
            OwHandleD()
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDropDiscardedValue,
    test_invalid_mismatched_branches_of_a_case_whose_value_is_taken,
    SppTypeMismatchError, R"(
    cls OwHandleD { }

    fun f(c: Bool) -> Void {
        let v = case c { OwHandleD() } else { 1_s32 }
    }
)");
