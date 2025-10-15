#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_moved,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)

        case 1 of {
            == 1 { let r = p }
            == 2 { }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_initialized,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p: Point
        case 1 of {
            == 1 { p = Point(x=5, y=6) }
            == 2 { }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_moved_1,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=6)
        case 1 of {
            == 1 { let x = p.x }
            == 2 { }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_moved_2,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=6)
        case 1 of {
            == 1 { let x = p.x }
            == 2 { let y = p.y }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_initialized_1,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let mut p = Point(x=5, y=6)
        let x = p.x

        case 1 of {
            == 1 { p.x = 123 }
            == 2 { }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_initialized_2,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let mut p = Point(x=5, y=6)
        let x = p.x
        let y = p.y

        case 1 of {
            == 1 { p.x = 123 }
            == 2 { p.y = 456 }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_1,
    SppInconsistentlyPinnedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    cor c(p: &Point) -> std::generator::Gen[std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)
        case 1 of {
            == 1 { c(&p) }
            == 2 { }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_2,
    SppInconsistentlyPinnedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    cor c(x: &std::number::S32) -> std::generator::Gen[std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)
        case 1 of {
            == 1 { c(&p.x) }
            == 2 { }
        }

        let r = p
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_3,
    SppInconsistentlyPinnedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    cor c(x: &std::number::S32) -> std::generator::Gen[std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)
        case 1 of {
            == 1 { c(&p.x) }
            == 2 { c(&p.y) }
        }

        let r = p
    }
)")
