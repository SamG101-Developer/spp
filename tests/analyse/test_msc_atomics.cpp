#include "../test_macros.hpp"

// REGRESSION TESTS

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_load_and_store_on_default_ordering, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[S32]::new(7)
        let b = a.load()
        a.store(9)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_load_with_an_explicit_ordering, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[S32]::new(7)
        let b = a.load[7_u8]()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_exchange_and_fetch_family, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[S32]::new(7)
        let b = a.exchange(1)
        let c = a.fetch_and(3)
        let d = a.fetch_or(1)
        let e = a.fetch_xor(2)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_compare_exchange_returns_a_pair, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[S32]::new(5)
        let (previous, swapped) = a.compex(5, 6)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_compare_exchange_weak_returns_a_pair, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[S32]::new(5)
        let (previous, swapped) = a.compex_weak(5, 6)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_atomics_at_several_widths, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[U8]::new(1_u8)
        let b = std::threading::atomic::Atom[U64]::new(1_u64)
        let c = a.load()
        let d = b.load()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestAtomics,
  test_valid_zeroed_atomic, R"(
    fun f() -> Void {
        let a = std::threading::atomic::Atom[S32]::new_zeroed()
        let b = a.load()
        std::mem::ops::drop(a)
    }
)");
