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

// A "skip" starts another iteration of the loop it continues, here the inner one: the outer body always returns, so
// it never runs twice, and "x" is moved at most once.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstMemoryLoopChecks,
  test_valid_inner_skip_does_not_continue_the_outer_loop, R"(
    fun f(b: Bool) -> Void {
        let x = Str::from("a")
        loop b {
            loop b {
                skip
            }
            std::mem::ops::drop(x)
            ret
        }
        std::mem::ops::drop(x)
    }
)");

// "exit skip" continues the outer loop, so its body can run again, with "x" already moved.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryLoopChecks,
  test_invalid_exit_skip_continues_the_outer_loop,
  SppUninitializedMemoryUseError, R"(
    fun f(b: Bool) -> Void {
        let x = Str::from("a")
        loop b {
            std::mem::ops::drop(x)
            loop b {
                exit skip
            }
            ret
        }
    }
)");

// A "let" inside the body declares its binding afresh each iteration, so its one value is given again each time.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_immutable_let_declared_and_given_each_iteration, R"(
    fun f() -> Void {
        let mut i = 0
        loop i < 3 {
            let x: S32
            x = 1
            i = i + x
        }
    }
)");

// The iterator behind "loop ... in" is dropped after the loop, which ends its loans there.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_iterator_loans_end_after_the_loop, R"(
    fun f() -> Void {
        let mut v = Vec[S32]::new()
        v.append(1)
        loop x in v.iter_mut() { x@ = 2 }
        v.append(3)
        drop(v)
    }
)");

// Dropped on a "ret" out of the loop too, by then: what it borrows cannot be consumed while the loop still holds it.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_value_dropped_inside_a_loop_borrowing_it,
    SppMovingEscapingBorrowedMemoryError, R"(
    fun f() -> S32 {
        let mut v = Vec[S32]::new()
        v.append(1)
        loop x in v.iter_ref() {
            case x@ == 1 {
                drop(v)
                ret 5
            }
        }
        drop(v)
        ret 0
    }
)");

// A "skip" is a way into the next iteration, with everything it leaves: a part moved before it is still moved when
// the body runs again.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_skip_leaves_a_part_moved,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f(c: Bool) -> Void {
        let mut p = Point(x=Str::from("5"), y=Str::from("5"))
        loop c {
            let x = p.x
            std::mem::ops::drop(x)
            case c { skip }
            p.x = Str::from("6")
        }
        std::mem::ops::drop(p)
    }
)");

// The same for a loan: a borrow taken on the way to a "skip" is held when the next iteration begins (through the
// "skip", not the body's end, so held on some ways back round).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_skip_leaves_a_loan_held,
    SppInconsistentlyEscapingBorrows, R"(
    fun f(c: Bool) -> Void {
        let mut v = std::vector::Vec[S32]::from(&[1, 2])
        let mut w = std::vector::Vec[S32]::from(&[3, 4])
        {
            let mut a = w[mut 0_uz]
            loop c {
                let n = v[mut 1_uz]
                a@ = 1
                case c {
                    a = v[mut 0_uz]
                    skip
                }
                a = w[mut 0_uz]
            }
        }
        std::mem::ops::drop(v)
        std::mem::ops::drop(w)
    }
)");

// A "skip" after the value is given again leaves nothing moved for the next iteration.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_skip_after_reinitialization, R"(
    fun f(c: Bool) -> Void {
        let mut x = Str::from("hello")
        loop c {
            std::mem::ops::drop(x)
            x = Str::from("world")
            case c { skip }
        }
        std::mem::ops::drop(x)
    }
)");
