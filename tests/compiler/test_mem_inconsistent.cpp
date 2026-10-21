#include "test_macros.hpp"

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
            == 1 {
                let r = p
                std::mem::ops::drop(r)
            }
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
            == 1 {
                let x = p.x
                std::mem::ops::drop(x)
            }
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
            == 1 {
                let x = p.x
                std::mem::ops::drop(x)
            }
            == 2 {
                let y = p.y
                std::mem::ops::drop(y)
            }
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
  test_invalid_memory_move_escaping_borrowed_value_in_branch,
  SppMovingEscapingBorrowedMemoryError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &Point) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let g = c(&p)
        case 1 of {
            == 1 { std::mem::ops::drop(p) }
            == 2 { }
        }
        std::mem::ops::drop(&g)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_move_escaping_borrowed_attribute_in_branch,
  SppMovingEscapingBorrowedMemoryError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let g = c(&p.x)
        case 1 of {
            == 1 { std::mem::ops::drop(p.x) }
            == 2 { }
        }
        std::mem::ops::drop(&g)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_move_escaping_borrow_container_in_branch,
  SppMovingEscapingBorrowedMemoryError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &Point) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let g = c(&p)
        case 1 of {
            == 1 { let h = g }
            == 2 { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_conflicting_borrow_in_branch_while_escaping_borrow_live,
  SppMemoryOverlapUsageError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &mut Point) -> Gen[Bool] { }

    fun g(p: &Point) -> Void { }

    fun f() -> Void {
        let mut p = Point(x=Str::from("5"), y=Str::from("5"))
        let h = c(&mut p)
        case 1 of {
            == 1 { g(&p) }
            == 2 { }
        }
        std::mem::ops::drop(&h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstMemoryInconsistent,
  test_valid_memory_branch_local_escaping_borrow_released, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &Point) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let x = c(&p)
                std::mem::ops::drop(x)
            }
            == 2 { }
        }

        let r = p
        std::mem::ops::drop(r)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstMemoryInconsistent,
  test_valid_memory_branch_local_escaping_borrow_of_attribute_released, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let x = c(&p.x)
                std::mem::ops::drop(x)
            }
            == 2 { }
        }

        let r = p
        std::mem::ops::drop(r)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstMemoryInconsistent,
  test_valid_memory_branch_local_escaping_borrows_of_different_attributes_released, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let x = c(&p.x)
                std::mem::ops::drop(x)
            }
            == 2 {
                let x = c(&p.y)
                std::mem::ops::drop(x)
            }
        }

        let r = p
        std::mem::ops::drop(r)
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
            == 1 {
                let r = p
                std::mem::ops::drop(r)
            }
            == 2 {
                let r = p
                std::mem::ops::drop(r)
            }
            else {
                let r = p
                std::mem::ops::drop(r)
            }
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
            else { p = Point(x=Str::from("7"), y=Str::from("7")) }
        }

        let r = p
        std::mem::ops::drop(r)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstMemoryInconsistent,
  test_valid_memory_borrow_after_branch_local_mut_escaping_borrow_released, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &mut Point) -> Gen[Bool] { }

    fun g(p: &Point) -> Void { }

    fun f() -> Void {
        let mut p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let h = c(&mut p)
                std::mem::ops::drop(h)
            }
            == 2 { }
        }

        g(&p)
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_inconsistently_escaping_borrow_from_outer_handle,
  SppInconsistentlyEscapingBorrows, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(p: &Point) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let h: Gen[Bool]
        case 1 of {
            == 1 { h = c(&p) }
            == 2 { }
        }

        let r = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_inconsistently_escaping_borrow_of_different_attributes_from_outer_handle,
  SppInconsistentlyEscapingBorrows, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let h: Gen[Bool]
        case 1 of {
            == 1 { h = c(&p.x) }
            == 2 { h = c(&p.y) }
        }

        let r = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_move_escaping_borrowed_by_outer_handle_after_case,
  SppMovingEscapingBorrowedMemoryError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    cor c(x: &Str) -> Gen[Bool] { }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let h: Gen[Bool]
        case 1 of {
            == 1 { h = c(&p.x) }
            == 2 { h = c(&p.x) }
            else { h = c(&p.x) }
        }

        let r = p
        std::mem::ops::drop(&h)
    }
)");

// Paths are joined by place: the same part taken on every path is taken, wherever each path took it. Comparing where
// the moves were written made two branches that each take "p.x" disagree, and "p.y" unusable after them.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstMemoryInconsistent,
  test_valid_memory_same_part_moved_on_every_path_then_sibling_used, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let x = p.x
                std::mem::ops::drop(x)
            }
            else {
                let x = p.x
                std::mem::ops::drop(x)
            }
        }

        let y = p.y
        std::mem::ops::drop(y)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_same_part_moved_on_every_path_then_used,
  SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let x = p.x
                std::mem::ops::drop(x)
            }
            else {
                let x = p.x
                std::mem::ops::drop(x)
            }
        }

        let x = p.x
        std::mem::ops::drop(x)
        let y = p.y
        std::mem::ops::drop(y)
    }
)");

// Todo: A part taken on only one path is still owned on the other, so "p.x" leaks when the "else" runs. Scope exit
//  counts a part taken on any path as taken (see "linear_utils::IsLive"), because a "case" without an "else" keeps its
//  entry state as a path even for a pattern that cannot fail. Fixed by the match and fall-through edges of M4.3.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_part_moved_on_one_path_leaks_on_the_other,
  SppLinearValueNotConsumedError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case 1 of {
            == 1 {
                let x = p.x
                std::mem::ops::drop(x)
            }
            else { }
        }

        let y = p.y
        std::mem::ops::drop(y)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstMemoryInconsistent,
  test_invalid_memory_partially_moved_in_nested_case,
  SppInconsistentlyInitializedMemoryUseError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }
    fun f(c: Bool, d: Bool) -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        case c {
            case d {
                std::mem::ops::drop(p.x)
            }
            else { }
        }
        else { }
        let r = p
    }
)");
