#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenForwarding,
  test_valid_gen_self_through_a_forwarding_superimposition, R"(
    cls Inner { !public v: S32 }
    cls Wrapper { !public inner: Inner }

    sup Wrapper ext std::ops::fwd::FwdRef[Inner] {
        cor fwd_ref(&self) -> std::generator::GenOnce[&Inner] {
            gen &self.inner
        }
    }

    sup Wrapper {
        !public cor get(&self) -> std::generator::GenOnce[&Inner] {
            gen self
        }
    }

    fun f() -> Void {
        let w = Wrapper(inner=Inner(v=1))
        let a = w.get()@.v
        std::mem::ops::drop(w)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenForwarding,
  test_valid_gen_the_forwarded_to_value_directly, R"(
    cls Inner { !public v: S32 }
    cls Wrapper { !public inner: Inner }

    sup Wrapper ext std::ops::fwd::FwdRef[Inner] {
        cor fwd_ref(&self) -> std::generator::GenOnce[&Inner] {
            gen &self.inner
        }
    }

    fun f() -> Void {
        let w = Wrapper(inner=Inner(v=1))
        let a = w.fwd_ref()@.v
        std::mem::ops::drop(w)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenForwarding,
  test_valid_mutable_forwarding_yields_a_mutable_borrow, R"(
    cls Inner { !public v: S32 }
    cls Wrapper { !public inner: Inner }

    sup Wrapper ext std::ops::fwd::FwdMut[Inner] {
        cor fwd_mut(&mut self) -> std::generator::GenOnce[&mut Inner] {
            gen &mut self.inner
        }
    }

    fun f() -> Void {
        let mut w = Wrapper(inner=Inner(v=1))
        w.fwd_mut()@.v = 2
        std::mem::ops::drop(w)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenForwarding,
  test_invalid_gen_a_value_that_does_not_forward,
  SppYieldedTypeMismatchError, R"(
    cls Inner { !public v: S32 }
    cls Wrapper { !public inner: Inner }

    sup Wrapper {
        cor get(&self) -> std::generator::GenOnce[&Inner] {
            gen self
        }
    }
)");

// TODO: Move these 2 tests - about "ret" inside a coroutine to end it.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenForwarding,
  test_valid_ret_ends_a_coroutine, R"(
    cor c() -> std::generator::Gen[S32] {
        gen 1
        ret
    }

    fun f() -> Void {
        loop x in c() { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenForwarding,
  test_valid_ret_early_from_a_coroutine, R"(
    cor c(stop: Bool) -> std::generator::Gen[S32] {
        case stop { ret }
        gen 1
        gen 2
    }

    fun f() -> Void {
        loop x in c(false) { }
    }
)");
