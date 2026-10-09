#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_loop_with_memory_move,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x = Str::from("hello world")
        loop true {
            let y = x
            std::mem::ops::drop(y)
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_loop_with_memory_move_nested,
    SppUninitializedMemoryUseError, R"(
    cls SomeType {
        !public a: Str
    }

    fun f() -> Void {
        let x = SomeType()
        loop true {
            let a = x.a
            std::mem::ops::drop(a)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_loop_with_memory_copy, R"(
    fun f() -> Void {
        let x = 123_u32
        loop true {
            let y = x
        }
    }
)");

// A non-copyable value moved inside the loop body is valid if it is re-initialized before the end of
// the iteration: the loop's memory pass runs the body twice, and the second pass sees a revived value.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_loop_with_memory_move_and_reinit, R"(
    fun f(b: Bool) -> Void {
        let mut x = Str::from("hello")
        loop b {
            let y = x
            x = Str::from("world")
            std::mem::ops::drop(y)
        }
        std::mem::ops::drop(x)
    }
)");

// The loop body is checked twice, and the state after the loop is the merge of every path out of it: the
// zero-iteration path, the body's end, and each "exit".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_uninitialized_let_assigned_only_inside_loop,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> S32 {
        let x: S32
        loop c {
            x = 1
            exit
        }
        ret x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_immutable_let_assigned_on_every_iteration,
    SppInvalidMutationError, R"(
    fun f(c: Bool) -> Void {
        let x: S32
        loop c {
            x = 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_skip_before_reinitialization,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> Void {
        let mut x = Str::from("hello")
        loop c {
            std::mem::ops::drop(x)
            case c { skip }
            x = Str::from("world")
        }
        std::mem::ops::drop(x)
    }
)");

// A body that always leaves on its first pass is never run a second time, so a move in it is a single move.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_move_in_body_that_always_exits, R"(
    fun f() -> Void {
        let x = Str::from("hello")
        loop true {
            std::mem::ops::drop(x)
            exit
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_move_in_nested_body_that_exits_both_loops, R"(
    fun f() -> Void {
        let x = Str::from("hello")
        loop true {
            loop true {
                std::mem::ops::drop(x)
                exit exit
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_moved_on_an_exit_then_used_after_the_loop,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> Void {
        let x = Str::from("a")
        loop c {
            case c {
                std::mem::ops::drop(x)
                exit
            }
        }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_two_exits_disagree_about_a_move,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> Void {
        let x = Str::from("a")
        loop true {
            case c {
                std::mem::ops::drop(x)
                exit
            }
            else { exit }
        }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_every_exit_moves_the_value, R"(
    fun f(c: Bool) -> Void {
        let x = Str::from("a")
        loop true {
            case c {
                std::mem::ops::drop(x)
                exit
            }
            else {
                std::mem::ops::drop(x)
                exit
            }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_only_exit_of_an_endless_loop_moves_the_value, R"(
    fun f(c: Bool) -> Void {
        let x = Str::from("a")
        loop true {
            case c {
                std::mem::ops::drop(x)
                exit
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_initialized_on_one_exit_only,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> S32 {
        let x: S32
        loop true {
            case c {
                x = 1
                exit
            }
            else { exit }
        }
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_initialized_on_every_exit, R"(
    fun f(c: Bool) -> S32 {
        let x: S32
        loop true {
            case c {
                x = 1
                exit
            }
            else {
                x = 2
                exit
            }
        }
        ret x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_moved_on_an_exit_of_an_outer_loop,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> Void {
        let x = Str::from("a")
        loop c {
            loop c {
                case c {
                    std::mem::ops::drop(x)
                    exit exit
                }
                exit
            }
        }
        std::mem::ops::drop(x)
    }
)");
