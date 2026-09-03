#include "../test_macros.hpp"

// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDiscardedValue,
    test_invalid_array_literal_as_statement,
    SppDiscardedValueError, R"(
    fun f() -> Void {
        [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDiscardedValue,
    test_invalid_identifier_as_statement,
    SppDiscardedValueError, R"(
    fun f() -> Void {
        let x = 123
        x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDiscardedValue,
    test_invalid_call_returning_value_as_statement,
    SppDiscardedValueError, R"(
    fun g() -> S32 { ret 1 }

    fun f() -> Void {
        g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDiscardedValue,
    test_valid_void_call_as_statement, R"(
    fun g() -> Void { }

    fun f() -> Void {
        g()
    }
)");

// The discard follows the "case" inwards: the error belongs on the "1" and the "2" that the branches end on, not on
// the "case" itself, because that is where a value nothing takes is actually produced.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDiscardedValue,
    test_invalid_case_as_statement_reports_branch_finals,
    SppDiscardedValueError, R"(
    fun f() -> Void {
        let c = true
        case c { 1 } else { 2 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDiscardedValue,
    test_valid_case_as_statement_with_void_branches, R"(
    fun g() -> Void { }

    fun f() -> Void {
        let c = true
        case c { g() } else { g() }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDiscardedValue,
    test_valid_case_bound_by_let, R"(
    fun f() -> Void {
        let c = true
        let x = case c { 1 } else { 2 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDiscardedValue,
    test_invalid_block_as_statement,
    SppDiscardedValueError, R"(
    fun f() -> Void {
        { 1 }
    }
)");
