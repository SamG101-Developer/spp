#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy, R"(
    fun f() -> std::void::Void {
        let x = 123_uz
        let a = x
        let b = x
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_custom_type, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    sup Point ext std::copy::Copy { }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)
        let a = p
        let b = p
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryCopy,
    test_valid_memory_copy_custom_generic_type, R"(
    cls Point[T] {
        x: T
        y: T
    }

    sup [T] Point[T] ext std::copy::Copy { }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)
        let a = p
        let b = p
    }
)")
