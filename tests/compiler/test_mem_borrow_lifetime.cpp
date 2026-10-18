#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_assign_borrow_extends_lifetime_indirect,
  SppBorrowLifetimeIncreaseError, R"(
    cor c(s: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x: Gen[&StrView]
        {
            let s = Str::from("hello")
            let coro = c(&s)
            x = coro
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_assign_borrow_extends_lifetime_direct,
  SppBorrowLifetimeIncreaseError, R"(
    cor c(s: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x: Gen[&StrView]
        {
            let s = Str::from("hello")
            x = c(&s)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_valid_memory_assign_borrow_non_extends_lifetime_indirect, R"(
    cor c(s: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let s = Str::from("hello")
        {
            let x: Gen[&StrView]
            {
                let coro = c(&s)
                x = coro
            }
            std::mem::ops::drop(x)
        }
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_valid_memory_assign_borrow_non_extends_lifetime_direct, R"(
    cor c(s: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let s = Str::from("hello")
        {
            let x: Gen[&StrView]
            {
                x = c(&s)
            }
            std::mem::ops::drop(x)
        }
        std::mem::ops::drop(s)
    }
)");

// A coroutine that contains MULTIPLE escaping borrows: the lifetime check must iterate every
// contained borrow, so if any one of them out-lives its source it is an error.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_multiple_borrows_extend_lifetime,
  SppBorrowLifetimeIncreaseError, R"(
    cor c(a: &Str, b: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x: Gen[&StrView]
        {
            let s1 = Str::from("a")
            let s2 = Str::from("b")
            let coro = c(&s1, &s2)
            x = coro
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_valid_memory_multiple_borrows_no_extend_lifetime, R"(
    cor c(a: &Str, b: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let s1 = Str::from("a")
        let s2 = Str::from("b")
        {
            let x: Gen[&StrView]
            {
                let coro = c(&s1, &s2)
                x = coro
            }
            std::mem::ops::drop(x)
        }
        std::mem::ops::drop(s1)
        std::mem::ops::drop(s2)
    }
)");

// Whatever shape the value has, a holder given a loan cannot outlive the value the loan borrows (M9.1). The old check
// only looked at an assignment whose value was a name or a postfix expression.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_closure_capture_extends_lifetime,
  SppBorrowLifetimeIncreaseError, R"(
    fun f() -> Void {
        let c: FunMut[(), Void]
        {
            let mut x = 1_s32
            c = (caps &mut x) { x@ = 2_s32 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_case_value_extends_lifetime,
  SppBorrowLifetimeIncreaseError, R"(
    cor co(s: &S32) -> Gen[S32] {
        gen 0
    }

    fun f(b: Bool) -> Void {
        let x: Gen[S32]
        {
            let s = 1_s32
            x = case b { co(&s) } else { co(&s) }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_block_value_extends_lifetime,
  SppBorrowLifetimeIncreaseError, R"(
    cor co(s: &S32) -> Gen[S32] {
        gen 0
    }

    fun f() -> Void {
        let x: Gen[S32]
        {
            let s = 1_s32
            x = { co(&s) }
        }
    }
)");

// A "let" whose value is a block can borrow something the block declares, which ends before the binding does.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_invalid_memory_let_of_a_block_borrowing_its_own_local,
  SppBorrowLifetimeIncreaseError, R"(
    cor co(s: &S32) -> Gen[S32] {
        gen 0
    }

    fun f() -> Void {
        let x = {
            let s = 1_s32
            co(&s)
        }
    }
)");

// Within the value's own lifetime, every one of those shapes is fine.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemoryBorrowLifetime,
  test_valid_memory_closure_capture_within_lifetime, R"(
    fun f() -> Void {
        let mut x = 1_s32
        {
            let c: FunMut[(), Void]
            {
                c = (caps &mut x) { x@ = 2_s32 }
            }
            std::mem::ops::drop(c)
        }
    }
)");
