#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMemoryEscpaingBorrows,
    test_invalid_memory_escaping_borrows_conflicting_borrow_mut_mut,
    SppMemoryOverlapUsageError, R"(
    cor c(a: &mut Str) -> Gen[StrView] {
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
    cor c1(a: &mut Str) -> Gen[StrView] {
        gen "0"
    }

    cor c2(a: &Str) -> Gen[StrView] {
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
    cor c1(a: &Str) -> Gen[StrView] {
        gen "0"
    }

    cor c2(a: &mut Str) -> Gen[StrView] {
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
    cor c(a: &Str) -> Gen[StrView] {
        gen "0"
    }

    fun f() -> Void {
        let mut x = Str::from("123")
        let coro1 = c(&x)
        let coro2 = c(&x)
    }
)");
