#include "../test_macros.hpp"
import testex;



SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_loop_with_memory_move,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let x = "hello world"
        loop true {
            let y = x
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_invalid_loop_with_memory_move_nested,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let x = "hello world"
        loop true {
            let d = x.data
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryLoopChecks,
    test_valid_loop_with_memory_copy, R"(
    fun f() -> std::void::Void {
        let x = 123_u32
        loop true {
            let y = x
        }
    }
)");
