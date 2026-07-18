#include "../test_macros.hpp"

// Type forwarding (std::ops::fwd::FwdRef / FwdMut): a type can forward to another type so that
// operations not defined on it resolve on the forwarded-to type. The canonical cases are
// Vec[T]/Arr[T] -> &View[T] / &mut View[T], and Str -> &StrView. These tests check that every kind
// of operation resolves through forwarding: member access, indexing, slicing, iteration (yielding),
// and binary operators / equality.

// --- Vec[T] forwards to View[T] -------------------------------------------------------------------

// Member access: `len`/`is_empty` are defined on View, not Vec, so they resolve via forwarding.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_method_len, R"(
    fun f(v: Vec[S32]) -> Void {
        let mut n = v.len()
        n = 0_uz
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_method_is_empty, R"(
    fun f(v: Vec[S32]) -> Void {
        let mut b = v.is_empty()
        b = false
    }
)");

// Indexing: `IndexRef` is superimposed on View, forwarded from Vec.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_indexing, R"(
    fun f(v: Vec[S32]) -> Void {
        let x = v[0_uz]
    }
)");

// Slicing: `SliceRef` is on View, forwarded from Vec.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_slicing, R"(
    fun f(v: Vec[S32]) -> Void {
        let s = v[0_uz to 2_uz]
    }
)");

// Iteration (yielding): `iter_ref`/`iter_mut` are on View, forwarded from Vec.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_iteration_ref, R"(
    fun f(v: Vec[S32]) -> Void {
        loop x in v.iter_ref() { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_iteration_mut, R"(
    fun f(mut v: Vec[S32]) -> Void {
        loop x in v.iter_mut() { }
    }
)");

// --- Str forwards to StrView ----------------------------------------------------------------------

// Member access on Str resolves on StrView via forwarding.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_method_len, R"(
    fun f(s: Str) -> Void {
        let mut n = s.len()
        n = 0_uz
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_method_contains, R"(
    fun f(s: Str) -> Void {
        let mut b = s.contains(&"lo")
        b = false
    }
)");

// Binary operator / equality: Str has no `Eq`; `==` forwards to StrView's `eq`.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_equality, R"(
    fun f(s: Str) -> Void {
        let mut b = s == Str::from("hello")
        b = false
    }
)");

// Iteration (yielding) over a forwarded coroutine.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_chars_iteration, R"(
    fun f(s: Str) -> Void {
        loop c in s.chars() { }
    }
)");

// --- Forwarding is a general mechanism, not special-cased to std types ----------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_custom_type_method, R"(
    use std::ops::fwd::FwdRef

    cls Inner { }
    sup Inner {
        !public fun greet(&self) -> Bool { ret true }
    }

    cls Wrapper { !public inner: Inner }
    sup Wrapper ext FwdRef[Inner] {
        cor fwd_ref(&self) -> GenOnce[&Inner] { gen &self.inner }
    }

    fun f(w: Wrapper) -> Void {
        let mut b = w.greet()
        b = false
    }
)");
