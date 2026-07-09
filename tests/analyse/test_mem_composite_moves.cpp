#include "../test_macros.hpp"

// Move tracking must recurse into composite literals (tuples/arrays): moving the same non-copyable
// symbol into two elements of a single literal is a use-after-move on the second element
// (ValidateSymbolMemory recurses into TupleLiteralAst/ArrayLiteral*Ast with mark_moves=true).

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_invalid_memory_double_move_into_tuple,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x = Str::from("a")
        let t = (x, x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_invalid_memory_double_move_into_array,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x = Str::from("a")
        let arr = [x, x]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_invalid_memory_use_after_move_into_tuple,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x = Str::from("a")
        let t = (x,)
        let y = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_invalid_memory_use_after_move_into_array,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x = Str::from("a")
        let arr = [x]
        let y = x
    }
)");

// A copyable element can appear multiple times in a composite literal: copyable types are not moved,
// so no use-after-move is raised.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_valid_memory_copyable_repeated_in_tuple, R"(
    fun f() -> Void {
        let x = true
        let t = (x, x)
        let y = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_valid_memory_copyable_repeated_in_array, R"(
    fun f() -> Void {
        let x = 123
        let arr = [x, x, x]
        let y = x
    }
)");

// Distinct non-copyable symbols in one literal is fine (no overlap between different symbols).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCompositeMoves,
    test_valid_memory_distinct_symbols_in_tuple, R"(
    fun f() -> Void {
        let x = Str::from("a")
        let y = Str::from("b")
        let t = (x, y)
    }
)");