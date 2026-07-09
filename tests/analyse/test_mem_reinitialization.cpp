#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryReinitialization,
    test_valid_memory_reinitialize_after_move, R"(
    fun f() -> Void {
        let mut x = Str::from("a")
        let y = x
        x = Str::from("b")
        let z = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryReinitialization,
    test_valid_memory_reinitialize_after_move_into_function, R"(
    fun consume(s: Str) -> Void { }
    fun f() -> Void {
        let mut x = Str::from("a")
        consume(x)
        x = Str::from("b")
        consume(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryReinitialization,
    test_valid_memory_reinitialize_attribute_after_partial_move_then_use_whole, R"(
    cls A { !public str: StrView }
    cls B { !public a: A }
    fun f() -> Void {
        let mut b = B()
        let x = b.a
        b.a = A()
        let y = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryReinitialization,
    test_valid_memory_reinitialize_after_move_multiple_cycles, R"(
    fun consume(s: Str) -> Void { }
    fun f() -> Void {
        let mut x = Str::from("a")
        consume(x)
        x = Str::from("b")
        consume(x)
        x = Str::from("c")
        consume(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryReinitialization,
    test_invalid_memory_use_whole_after_reinitializing_only_one_partial_move,
    SppPartiallyInitializedMemoryUseError, R"(
    cls Inner { !public str: StrView }
    cls A { !public a: Inner, !public b: Inner }
    fun f() -> Void {
        let mut x = A()
        let p = x.a
        let q = x.b
        x.a = Inner()
        let y = x
    }
)");
