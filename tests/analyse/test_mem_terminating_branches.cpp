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
    SppUninitializedMemoryUseError, R"(
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
