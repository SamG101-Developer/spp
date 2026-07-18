#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_initialized_moved,
    SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))

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
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p: Point
        case 1 of {
            == 1 { p = Point(x=Str::from("5"), y=Str::from("5")) }
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
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
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
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
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
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let mut p = Point(x=Str::from("5"), y=Str::from("5"))
        let x = p.x

        case 1 of {
            == 1 { p.x = Str::from("6") }
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
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let mut p = Point(x=Str::from("5"), y=Str::from("5"))
        let x = p.x
        let y = p.y

        case 1 of {
            == 1 { p.x = Str::from("6") }
            == 2 { p.y = Str::from("6") }
        }

        let r = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_1,
    SppInconsistentlyEscapingBorrows, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &Point) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 { let x = c(&p) }
            == 2 { }
        }

        let r = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_2,
    SppInconsistentlyEscapingBorrows, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 { let x = c(&p.x) }
            == 2 { }
        }

        let r = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryInconsistent,
    test_invalid_memory_inconsistently_pinned_3,
    SppInconsistentlyEscapingBorrows, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 { let x = c(&p.x) }
            == 2 { let x = c(&p.y) }
        }

        let r = p
    }
)");

// Positive counterparts: when every branch leaves a symbol in the SAME memory state, there is no
// inconsistency, so the post-case state is applied cleanly.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryInconsistent,
    test_valid_memory_consistently_moved, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 { let r = p }
            == 2 { let r = p }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryInconsistent,
    test_valid_memory_consistently_initialized, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p: Point
        case 1 of {
            == 1 { p = Point(x=Str::from("5"), y=Str::from("5")) }
            == 2 { p = Point(x=Str::from("6"), y=Str::from("6")) }
        }

        let r = p
    }
)");
