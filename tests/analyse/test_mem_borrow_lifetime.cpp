#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMemoryBorrowLifetime,
    test_invalid_memory_assign_borrow_extends_lifetime_indirect,
    SppBorrowLifetimeIncreaseError, R"(
    cor c(s: &Str) -> Gen[StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x: Gen[StrView]
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
    cor c(s: &Str) -> Gen[StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x: Gen[StrView]
        {
            let s = Str::from("hello")
            x = c(&s)
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestMemoryBorrowLifetime,
    test_valid_memory_assign_borrow_non_extends_lifetime_indirect, R"(
    cor c(s: &Str) -> Gen[StrView] {
        gen "0"
    }

    fun f() -> Void {
        let s = Str::from("hello")
        let x: Gen[StrView]
        {
            let coro = c(&s)
            x = coro
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestMemoryBorrowLifetime,
    test_valid_memory_assign_borrow_non_extends_lifetime_direct, R"(
    cor c(s: &Str) -> Gen[StrView] {
        gen "0"
    }

    fun f() -> Void {
        let s = Str::from("hello")
        let x: Gen[StrView]
        {
            x = c(&s)
        }
    }
)");
