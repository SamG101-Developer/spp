#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryEscapingBorrows,
  test_invalid_memory_escaping_borrows_conflicting_borrow_mut_mut,
  SppMemoryOverlapUsageError, R"(
    cor c(a: &mut Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let mut x = Str::from("123")
        let coro1 = c(&mut x)
        let coro2 = c(&mut x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryEscapingBorrows,
  test_invalid_memory_escaping_borrows_conflicting_borrow_mut_ref,
  SppMemoryOverlapUsageError, R"(
    cor c1(a: &mut Str) -> Gen[&StrView] {
        gen "0"
    }

    cor c2(a: &Str) -> Gen[&StrView] {
        gen "1"
    }

    fun f() -> Void {
        let mut x = Str::from("123")
        let coro1 = c1(&mut x)
        let coro2 = c2(&x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryEscapingBorrows,
  test_invalid_memory_escaping_borrows_conflicting_borrow_ref_mut,
  SppMemoryOverlapUsageError, R"(
    cor c1(a: &Str) -> Gen[&StrView] {
        gen "0"
    }

    cor c2(a: &mut Str) -> Gen[&StrView] {
        gen "1"
    }

    fun f() -> Void {
        let mut x = Str::from("123")
        let coro1 = c1(&x)
        let coro2 = c2(&mut x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemoryEscapingBorrows,
  test_valid_memory_escaping_borrows_conflicting_borrow_ref_ref, R"(
    cor c(a: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let mut x = Str::from("123")
        {
            let coro1 = c(&x)
            let coro2 = c(&x)
        }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_let,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f() -> Void {
        let x = Str::from("hello world")
        let coroutine = g(&x)
        let y = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_assign_variable,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f() -> Void {
        let y: Str
        let x = Str::from("hello world")
        let coroutine = g(&x)
        y = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_assign_attribute,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    cls A {
        !public a: Str
    }

    fun f(mut a: A) -> Void {
        let y: Str
        let x = Str::from("hello world")
        let coroutine = g(&x)
        a.a = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_func_call,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f(x: Str) -> Void { }

    fun h() -> Void {
        let x = Str::from("hello world")
        let coroutine = g(&x)
        f(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_object_init,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    cls A {
        !public a: Str
    }

    fun h() -> Void {
        let x = Str::from("hello world")
        let coroutine = g(&x)
        let a = A(a=x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_inner_scope_return,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun h() -> Void {
        let y = {
            let x = Str::from("hello world")
            let coroutine = g(&x)
            x
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_loop_escape,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun h() -> Void {
        let looped = loop true {
            let x = Str::from("hello world")
            let coroutine = g(&x)
            exit x
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_gen,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    cor h() -> Gen[Str, Bool] {
        let x = Str::from("hello world")
        let coroutine = g(&x)
        let sent = gen x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_moving_pinned_borrow_ret,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun h() -> Gen[Str, Bool] {
        let x = Str::from("hello world")
        let coroutine = g(&x)
        ret coroutine
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstEscapingBorrows,
  test_valid_memory_move_value_not_pinned, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f() -> Void {
        let x = Str::from("hello world")
        let y = Str::from("other")
        {
            let coroutine = g(&x)
            let z = y
            std::mem::ops::drop(z)
        }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstEscapingBorrows,
  test_valid_memory_move_after_pinned_borrow_scope_released, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f() -> Void {
        let x = Str::from("hello world")
        {
            let coroutine = g(&x)
        }
        let y = x
        std::mem::ops::drop(y)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestAstEscapingBorrows,
  test_invalid_memory_move_after_pinned_borrow_held_by_outer_handle,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f() -> Void {
        let x = Str::from("hello world")
        let coroutine: Gen[Str, Bool]
        {
            coroutine = g(&x)
        }
        let y = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAstEscapingBorrows,
  test_valid_memory_deref_of_indexed_element_holds_no_escaping_borrow, R"(
    fun f() -> Void {
        let mut xs = Vec[U32]()
        xs.append(1_u32)
        let a = xs[0_uz]@
        let b = xs
        std::mem::ops::drop(b)
    }
)");
