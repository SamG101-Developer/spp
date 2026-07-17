#include "../test_macros.hpp"

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
        let x: Gen[&StrView]
        {
            let coro = c(&s)
            x = coro
        }
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
        let x: Gen[&StrView]
        {
            x = c(&s)
        }
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
        let x: Gen[&StrView]
        {
            let coro = c(&s1, &s2)
            x = coro
        }
    }
)");
