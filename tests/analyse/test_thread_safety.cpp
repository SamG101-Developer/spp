#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_plain_type_is_thread_safe, R"(
    use std::threading::safe::ThreadSafe

    cls Plain { !public a: std::number::S32 }
    cls Box[T: ThreadSafe] { !public v: T }

    fun f() -> Void {
        let b = Box[Plain](v=Plain(a=1))
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_annotated_type_is_a_hazard,
  SppGenericConstraintError, R"(
    use std::annotations::thread_hazard
    use std::threading::safe::ThreadSafe

    !thread_hazard
    cls Haz { }
    cls Box[T: ThreadSafe] { !public v: T }

    fun f() -> Void {
        let b = Box[Haz](v=Haz())
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_type_holding_a_hazard_is_a_hazard,
  SppGenericConstraintError, R"(
    use std::annotations::thread_hazard
    use std::threading::safe::ThreadSafe

    !thread_hazard
    cls Haz { }
    cls Holder { !public h: Haz }
    cls Box[T: ThreadSafe] { !public v: T }

    fun f() -> Void {
        let b = Box[Holder](v=Holder(h=Haz()))
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_container_of_hazards_is_a_hazard, SppGenericConstraintError, R"(
    use std::annotations::thread_hazard
    use std::threading::safe::ThreadSafe

    !thread_hazard
    cls Haz { }
    cls Box[T: ThreadSafe] { !public v: T }

    fun f() -> Void {
        let b = Box[std::vector::Vec[Haz]](v=std::vector::Vec[Haz]::new())
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_unconstrained_generic_is_not_thread_safe, SppGenericConstraintError, R"(
    use std::threading::safe::ThreadSafe

    cls Box[T: ThreadSafe] { !public v: T }

    fun forward[U](v: U) -> Void {
        let b = Box[U](v=v)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_constrained_generic_is_thread_safe, R"(
    use std::threading::safe::ThreadSafe

    cls Box[T: ThreadSafe] { !public v: T }

    fun forward[U: ThreadSafe](v: U) -> Void {
        let b = Box[U](v=v)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_borrow_of_a_hazard_is_a_hazard, SppFunctionCallNoValidSignaturesError, R"(
    use std::annotations::thread_hazard
    use std::threading::safe::ThreadSafe

    !thread_hazard
    cls Haz { }

    # Asked of a function generic rather than a class one, because a borrow cannot be a class field type at all.
    fun needs_safe[T: ThreadSafe](x: T) -> Void { }

    fun f(h: &Haz) -> Void {
        needs_safe(h)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_rc_is_a_hazard, SppGenericConstraintError, R"(
    use std::threading::safe::ThreadSafe

    cls Box[T: ThreadSafe] { !public v: T }

    fun f() -> Void {
        let b = Box[std::rc::rc::Rc[std::number::S32]](v=std::rc::rc::Rc[std::number::S32]::new(1))
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_capture_free_closure_is_thread_safe, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        needs_safe(() -> std::number::S32 { ret 1 })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_closure_capturing_a_safe_value_is_thread_safe, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        let n = 7
        needs_safe((caps n) -> std::number::S32 { ret n })
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_closure_capturing_a_hazard_is_not_thread_safe,
  SppFunctionCallNoValidSignaturesError, R"(
    use std::annotations::thread_hazard
    use std::threading::safe::ThreadSafe

    !thread_hazard
    cls Haz { }

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        let h = Haz()
        needs_safe((caps h) -> std::number::S32 {
            std::mem::ops::drop(h)
            ret 1
        })
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_closure_capturing_a_borrow_is_not_thread_safe,
  SppFunctionCallNoValidSignaturesError, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        let n = 7
        needs_safe((caps &n) -> std::void::Void { })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_borrow_capturing_closure_is_still_callable, R"(
    fun f() -> Void {
        let n = 7
        let g = (caps &n) -> std::void::Void { }
        g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_closure_still_matches_a_written_function_type, R"(
    fun take(g: std::function::FunRef[(), std::number::S32]) -> Void { std::mem::ops::drop(g) }

    fun f() -> Void {
        let g: std::function::FunRef[(), std::number::S32] = () -> std::number::S32 { ret 1 }
        let a = g()
        let b = g()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
        take(g)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_spawn_a_safe_closure, R"(
    fun f() -> Void {
        let h = std::threading::thread::spawn(() -> std::void::Void { }).expect("spawn failed")
        h.join().expect("join failed")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_spawn_a_closure_holding_a_mutex, R"(
    fun f() -> Void {
        let m = std::threading::mutex::Mutex[std::number::S32]::new(1)
        let h = std::threading::thread::spawn((caps m) -> std::void::Void { std::mem::ops::drop(m) })
            .expect("spawn failed")
        h.join().expect("join failed")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_spawn_a_closure_capturing_an_rc,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let r = std::rc::rc::Rc[std::number::S32]::new(1)
        let h = std::threading::thread::spawn((caps r) -> std::void::Void { std::mem::ops::drop(r) })
            .expect("spawn failed")
        h.join().expect("join failed")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_spawn_a_closure_holding_a_mutex_of_a_hazard,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let m = std::threading::mutex::Mutex[std::rc::rc::Rc[std::number::S32]]::new(
            std::rc::rc::Rc[std::number::S32]::new(1))
        let h = std::threading::thread::spawn((caps m) -> std::void::Void { std::mem::ops::drop(m) })
            .expect("spawn failed")
        h.join().expect("join failed")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_spawn_a_closure_returning_a_hazard,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let h = std::threading::thread::spawn(() -> std::rc::rc::Rc[std::number::S32] {
            ret std::rc::rc::Rc[std::number::S32]::new(1)
        }).expect("spawn failed")
        std::mem::ops::drop(h.join().expect("join failed"))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_arc_of_a_safe_value_is_thread_safe, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        needs_safe(std::threading::arc::Arc[std::number::S32]::new(1))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_arc_of_a_hazard_is_a_hazard,
  SppFunctionCallNoValidSignaturesError, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        needs_safe(std::threading::arc::Arc[std::rc::rc::Rc[std::number::S32]]::new(
            std::rc::rc::Rc[std::number::S32]::new(1)))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestThreadSafety,
  test_valid_threading_weak_is_thread_safe, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        let a = std::threading::arc::Arc[std::number::S32]::new(1)
        let w = a.downgrade()
        std::mem::ops::drop(a)
        needs_safe(w)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestThreadSafety,
  test_invalid_rc_weak_is_a_hazard, SppFunctionCallNoValidSignaturesError, R"(
    use std::threading::safe::ThreadSafe

    fun needs_safe[T: ThreadSafe](x: T) -> Void { std::mem::ops::drop(x) }

    fun f() -> Void {
        let r = std::rc::rc::Rc[std::number::S32]::new(1)
        let w = r.downgrade()
        std::mem::ops::drop(r)
        needs_safe(w)
    }
)");
