#include "../test_macros.hpp"

// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_live_at_return,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
        ret
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_live_at_return_from_nested_scope,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
        let c = true
        case c { ret } else { }
        let Handle(fd) = h
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearBranches,
    test_valid_value_consumed_before_return, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
        let Handle(fd) = h
        ret
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearBranches,
    test_valid_value_returned_counts_as_consumed, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Handle {
        let h = Handle(fd=1)
        ret h
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_live_at_loop_exit,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let mut i = 0
        loop i < 3 {
            let h = Handle(fd=1)
            exit
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_live_at_loop_skip,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let mut i = 0
        loop i < 3 {
            i += 1
            let h = Handle(fd=1)
            skip
        }
    }
)");

// A value consumed on some branches of a "case" and left alone on the others is still owned on those paths, so the
// scope end has to answer for it. The inconsistency is only reported when the value is used again after the "case";
// at the scope end "IsLive" reads the first branch's state, so which branch comes first decides the outcome.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_consumed_in_first_branch_only,
    SppLinearValueNotConsumedError, R"(
    cls BrHandleA { }

    fun eat(l: BrHandleA) -> Void { let BrHandleA() = l }

    fun f(c: Bool) -> Void {
        let l = BrHandleA()
        case c { eat(l) } else { }
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_consumed_in_two_of_three_branches,
    SppLinearValueNotConsumedError, R"(
    cls BrHandleB { }

    fun eat(l: BrHandleB) -> Void { let BrHandleB() = l }

    fun f(x: S32) -> Void {
        let l = BrHandleB()
        case x of {
            == 1 { eat(l) }
            == 2 { eat(l) }
            else { }
        }
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_consumed_in_case_without_else,
    SppLinearValueNotConsumedError, R"(
    cls BrHandleC { }

    fun eat(l: BrHandleC) -> Void { let BrHandleC() = l }

    fun f(c: Bool) -> Void {
        let l = BrHandleC()
        case c { eat(l) }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearBranches,
    test_invalid_value_consumed_in_else_branch_only,
    SppLinearValueNotConsumedError, R"(
    cls BrHandleD { }

    fun eat(l: BrHandleD) -> Void { let BrHandleD() = l }

    fun f(c: Bool) -> Void {
        let l = BrHandleD()
        case c { } else { eat(l) }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearBranches,
    test_valid_value_consumed_on_every_branch, R"(
    cls BrHandleE { }

    fun eat(l: BrHandleE) -> Void { let BrHandleE() = l }

    fun f(c: Bool) -> Void {
        let l = BrHandleE()
        case c { eat(l) } else { eat(l) }
    }
)");
