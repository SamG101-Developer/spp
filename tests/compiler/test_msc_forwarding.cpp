#include "test_macros.hpp"

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
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_method_is_empty, R"(
    fun f(v: Vec[S32]) -> Void {
        let mut b = v.is_empty()
        b = false
        std::mem::ops::drop(v)
    }
)");

// Indexing: `IndexRef` is superimposed on View, forwarded from Vec.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_indexing, R"(
    fun f(v: Vec[S32]) -> Void {
        {
            let x = v[0_uz]
        }
        std::mem::ops::drop(v)
    }
)");

// Slicing: `SliceRef` is on View, forwarded from Vec.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_slicing, R"(
    fun f(v: Vec[S32]) -> Void {
        {
            let s = v[0_uz to 2_uz]
        }
        std::mem::ops::drop(v)
    }
)");

// Iteration (yielding): `iter_ref`/`iter_mut` are on View, forwarded from Vec.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_iteration_ref, R"(
    fun f(v: Vec[S32]) -> Void {
        loop x in v.iter_ref() { }
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_vec_iteration_mut, R"(
    fun f(mut v: Vec[S32]) -> Void {
        loop x in v.iter_mut() { }
        std::mem::ops::drop(v)
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
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_method_contains, R"(
    fun f(s: Str) -> Void {
        let mut b = s.contains(&"lo")
        b = false
        std::mem::ops::drop(s)
    }
)");

// Binary operator / equality: Str has no `Eq`; `==` forwards to StrView's `eq`.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_equality, R"(
    fun f(s: Str) -> Void {
        let hello = Str::from("hello")
        let mut b = s == hello
        b = false
        std::mem::ops::drop(s)
        std::mem::ops::drop(hello)
    }
)");

// Iteration (yielding) over a forwarded coroutine.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_str_chars_iteration, R"(
    fun f(s: Str) -> Void {
        loop c in s.chars() { }
        std::mem::ops::drop(s)
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
        std::mem::ops::drop(w)
    }
)");

// --- "Self" in a method reached by forwarding -----------------------------------------------------

// A method declared on a class and called on an implementer of it has "Self" pinned to the implementer; the same holds
// when the implementer is reached by forwarding. The control calls it on the implementer directly.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_self_pinned_to_implementer_directly, R"(
    cls Writer { }
    sup Writer {
        !public !abstract_method
        fun write(&self) -> S32 { }

        !public
        fun accepts(&self, other: &Self) -> Bool { ret true }
    }

    cls Mine { }
    sup Mine ext Copy { }
    sup Mine ext Writer {
        fun write(&self) -> S32 { ret 7 }
    }

    fun f() -> Void {
        let m = Mine()
        let b = m.accepts(&Mine())
    }
)");

// "CallSelf" answers the forwarded-to type; it used to answer nothing, leaving "Self" the declaring "Writer", so
// "&Mine" was rejected (E1 under E51).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_self_pinned_to_implementer_through_forwarding, R"(
    use std::ops::fwd::FwdRef

    cls Writer { }
    sup Writer {
        !public !abstract_method
        fun write(&self) -> S32 { }

        !public
        fun accepts(&self, other: &Self) -> Bool { ret true }
    }

    cls Mine { }
    sup Mine ext Copy { }
    sup Mine ext Writer {
        fun write(&self) -> S32 { ret 7 }
    }

    cls Wrapper { !public inner: Mine }
    sup Wrapper ext FwdRef[Mine] {
        cor fwd_ref(&self) -> GenOnce[&Mine] { gen &self.inner }
    }

    fun f(w: Wrapper) -> Void {
        let b = w.accepts(&Mine())
        std::mem::ops::drop(w)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestForwarding,
    test_invalid_forwarding_through_an_early_return,
    SppLinearValueNotConsumedError, R"(
    fun g() -> Opt[Vec[U64]] { ret None }
    fun f() -> Opt[USize] {
        let n = g()?.len()
        ret Some(val=n)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestForwarding,
    test_valid_forwarding_static_member_access_on_type, R"(
    fun g(v: Vec[S32]) -> Void {
        let m = Vec[S32]::is_empty
        std::mem::ops::drop(v)
    }
)");
