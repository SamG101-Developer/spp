#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_moved,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))

        case 1 of {
            == 1 { let r = p }
            == 2 { }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_initialized,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    fun f() -> std::void::Void {
        let p: Point
        case 1 of {
            == 1 { p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5")) }
            == 2 { }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_moved_1,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        case 1 of {
            == 1 { let x = p.x }
            == 2 { }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_moved_2,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        case 1 of {
            == 1 { let x = p.x }
            == 2 { let y = p.y }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_initialized_1,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    fun f() -> std::void::Void {
        let mut p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        let x = p.x

        case 1 of {
            == 1 { p.x = std::string::Str::from("6") }
            == 2 { }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_partially_initialized_2,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    fun f() -> std::void::Void {
        let mut p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        let x = p.x
        let y = p.y

        case 1 of {
            == 1 { p.x = std::string::Str::from("6") }
            == 2 { p.y = std::string::Str::from("6") }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_1,
    SppInconsistentlyPinnedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    cor c(p: &Point) -> std::generator::Gen[std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        case 1 of {
            == 1 { c(&p) }
            == 2 { }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_2,
    SppInconsistentlyPinnedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    cor c(x: &std::string::Str) -> std::generator::Gen[std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        case 1 of {
            == 1 { c(&p.x) }
            == 2 { }
        }

        let r = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_3,
    SppInconsistentlyPinnedMemoryUseError, R"(
    cls Point {
        x: std::string::Str
        y: std::string::Str
    }

    cor c(x: &std::string::Str) -> std::generator::Gen[std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        case 1 of {
            == 1 { c(&p.x) }
            == 2 { c(&p.y) }
        }

        let r = p
    }
)");
