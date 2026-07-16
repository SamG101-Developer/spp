#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMemoryEscpaingBorrows,
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
    TestMemoryEscpaingBorrows,
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
    TestMemoryEscpaingBorrows,
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
    TestMemoryEscpaingBorrows,
    test_valid_memory_escaping_borrows_conflicting_borrow_ref_ref, R"(
    cor c(a: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let mut x = Str::from("123")
        let coro1 = c(&x)
        let coro2 = c(&x)
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
        loop true {
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
        gen x
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

// Positive counterparts: a value is only pinned while a coroutine actually holds a borrow to it.

// Moving a value that is NOT the one pinned by the coroutine is fine.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstEscapingBorrows,
    test_valid_memory_move_value_not_pinned, R"(
    cor g(a: &Str) -> Gen[Str, Bool] { }

    fun f() -> Void {
        let x = Str::from("hello world")
        let y = Str::from("other")
        let coroutine = g(&x)
        let z = y
    }
)");

// Once the coroutine holding the pin goes out of scope, the pin is released and the value can move.
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
    }
)");
