#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_loop_with_memory_move,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x = Str::from("hello world")
        loop true {
            let y = x
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
    fun f() -> Void {
        let mut x = Str::from("hello")
        loop true {
            let y = x
            x = Str::from("world")
        }
    }
)");
