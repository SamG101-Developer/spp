#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy, R"(
    fun f() -> Void {
        let x = 123_uz
        let a = x
        let b = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_custom_type, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    sup Point ext Copy { }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let a = p
        let b = p
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_custom_generic_type, R"(
    cls Point[T] {
        !public x: T
        !public y: T
    }

    sup [T] Point[T] ext Copy { }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let a = p
        let b = p
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_by_constrained_generic, R"(
    fun g[T: Copy](t: T) -> T {
        let x = t
        ret t
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_by_constrained_generic_layered, R"(
    cls CopyableType { }
    sup CopyableType ext Copy { }

    fun g[T: CopyableType](t: T) -> T {
        let x = t
        ret t
    }
)");

// Negative counterparts: without an opt-in Copy, a value is moved by default and cannot be reused.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryCopy,
    test_invalid_memory_non_copyable_struct_used_twice,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let a = p
        let b = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryCopy,
    test_invalid_memory_non_copyable_generic_moved_twice,
    SppUninitializedMemoryUseError, R"(
    fun g[T](t: T) -> T {
        let x = t
        ret t
    }
)");

// A copyable attribute can be read repeatedly out of a non-copyable struct (partial copy).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_copyable_attribute_from_non_copyable_struct, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let a = p.x
        let b = p.x
    }
)");
