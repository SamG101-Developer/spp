#include "../test_macros.hpp"

// Todo: Every case in this file is red until the standard library is migrated to linear ownership. The test project
//  compiles std, and std still leaves values unconsumed, so a SHOULD_PASS case fails on std rather than on its own
//  code and a SHOULD_FAIL case can throw the right error type for the wrong reason. They go green as std lands.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_value_moved_into_consumer, R"(
    cls Handle { !public fd: S32 }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        let h = Handle(fd=1)
        close(h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_copyable_local_left_live, R"(
    fun f() -> Void {
        let x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_borrow_parameter_not_consumed, R"(
    cls Handle { !public fd: S32 }

    fun peek(h: &Handle) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_local_left_live,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_local_left_live_in_nested_block,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        {
            let h = Handle(fd=1)
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_local_left_live_in_loop_body,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let mut i = 0
        loop i < 3 {
            let h = Handle(fd=1)
            i += 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_only_some_attributes_moved_off,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }
    cls Pair {
        !public a: Handle
        !public b: Handle
    }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        let p = Pair(a=Handle(fd=1), b=Handle(fd=2))
        let a = p.a
        close(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_all_attributes_moved_off, R"(
    cls Handle { !public fd: S32 }
    cls Pair {
        !public a: Handle
        !public b: Handle
    }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        let p = Pair(a=Handle(fd=1), b=Handle(fd=2))
        let a = p.a
        let b = p.b
        close(a)
        close(b)
    }
)");
