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
