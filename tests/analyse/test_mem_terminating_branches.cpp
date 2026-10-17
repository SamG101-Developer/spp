#include "../test_macros.hpp"

// A branch that leaves the scope does not hand its memory state to the code after the "case" - control never gets
// there. "RetStatementAst" declared "Terminates()" but "LoopControlFlowStatementAst" did not, so a branch ending in
// "skip" or "exit" read as falling through and every move it made was applied to the statements after the case. The
// "ret" form was covered and worked; the loop-control form was not covered and did not.
//
// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_valid_branch_ending_in_skip_does_not_leak_its_moves, R"(
    cls T { }

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(c: Bool) -> Void {
        let mut i = 0
        loop i < 3 {
            let v = T()
            case c {
                consume(v)
                skip
            }
            consume(v)
            i += 1
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_valid_branch_ending_in_exit_does_not_leak_its_moves, R"(
    cls T { }

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(c: Bool) -> Void {
        let mut i = 0
        loop i < 3 {
            let v = T()
            case c {
                consume(v)
                exit
            }
            consume(v)
            i += 1
        }
    }
)");

// The "ret" form of the same thing, which already worked - kept so the two stay honest about each other.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_valid_branch_ending_in_ret_does_not_leak_its_moves, R"(
    cls T { }

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(c: Bool) -> Void {
        let v = T()
        case c {
            consume(v)
            ret
        }
        consume(v)
    }
)");

// A branch that does *not* terminate really does hand its state on, so the value is gone by the second call.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_invalid_non_terminating_branch_leaks_its_moves,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls T { }

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(c: Bool) -> Void {
        let v = T()
        case c {
            consume(v)
        }
        consume(v)
    }
)");

// A disagreement nested in a branch is part of what that branch leaves behind: it survives the merge with the other
// branches, whichever branch it is in.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_invalid_nested_inconsistency_in_the_second_branch_survives_the_merge,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> Void {
        let x: S32
        case c {
            x = 1_s32
        }
        else {
            case c { x = 2_s32 }
        }
        let y = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_invalid_nested_inconsistency_in_the_first_branch_survives_the_merge,
    SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(c: Bool) -> Void {
        let x: S32
        case c {
            case c { x = 2_s32 }
        }
        else {
            x = 1_s32
        }
        let y = x
    }
)");

// A disagreement inside one branch does not carry into the next: each branch starts from the state before the "case".
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryTerminatingBranches,
    test_valid_nested_inconsistency_does_not_leak_into_the_next_branch, R"(
    fun f(c: Bool) -> Void {
        let x: S32
        case c {
            case c { x = 1_s32 }
        }
        else {
            x = 2_s32
            let y = x
        }
    }
)");
