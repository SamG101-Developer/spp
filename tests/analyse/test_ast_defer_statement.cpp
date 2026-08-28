#include "../test_macros.hpp"

// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp. The test project
//  compiles std, so a SHOULD_PASS case fails on std rather than on its own code, and a SHOULD_FAIL case can throw the
//  right error type for the wrong reason.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  DeferStatementAst,
  test_valid_defer_discharges_at_scope_end, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        !public
        fun close(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let h = Handle(fd=1)
        defer h.close()
    }
)");

// The point of the feature: an early exit does not get to abandon what the scope was holding.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  DeferStatementAst,
  test_valid_defer_covers_early_return, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        !public
        fun close(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f(early: Bool) -> Void {
        let h = Handle(fd=1)
        defer h.close()
        case early { ret }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  DeferStatementAst,
  test_valid_defer_covers_loop_exit, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        !public
        fun close(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let mut i = 0
        loop i < 3 {
            let h = Handle(fd=1)
            defer h.close()
            exit
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  DeferStatementAst,
  test_valid_two_defers, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        !public
        fun close(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let a = Handle(fd=1)
        let b = Handle(fd=2)
        defer a.close()
        defer b.close()
    }
)");

// A "defer" only runs if control reached it. The whole scope's defers are registered up-front, so an exit part-way
// through has to run the ones above it and skip the ones below - which never registered anything, and whose values
// have not been allocated yet at that point. Emitting those crashed codegen with "target identifier has no
// allocation".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  DeferStatementAst,
  test_valid_defer_below_an_early_exit_does_not_run, R"(
    cls T { }

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(early: Bool) -> Void {
        let a = T()
        defer consume(a)
        case early { ret }
        let b = T()
        defer consume(b)
    }
)");

// A "?" leaves on its failure branch only, so "Terminates" does not report it - but it still expands to a "ret", and a
// deferred expression is replayed at every exit of its scope, so the return would be emitted inside the return already
// in progress. This crashed codegen before it was rejected here.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  DeferStatementAst,
  test_invalid_defer_early_return,
  SppDeferTerminatesError, R"(
    cls E { }

    fun g() -> std::result::Res[Void, E] { ret std::result::Pass[Void]() }

    fun f() -> std::result::Res[Void, E] {
        defer g()?
        ret std::result::Pass[Void]()
    }
)");

// ...at any depth, not just as the outermost operator.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  DeferStatementAst,
  test_invalid_defer_nested_early_return,
  SppDeferTerminatesError, R"(
    cls E { }

    fun g() -> std::result::Res[Void, E] { ret std::result::Pass[Void]() }
    fun h(v: Void) -> Void { }

    fun f() -> std::result::Res[Void, E] {
        defer h(g()?)
        ret std::result::Pass[Void]()
    }
)");

// A "?" inside a closure written in a deferred expression returns from the closure, not from the deferring function,
// so it is past the point the restriction applies to.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  DeferStatementAst,
  test_valid_defer_early_return_inside_a_closure, R"(
    cls E { }

    fun g() -> std::result::Res[Void, E] { ret std::result::Pass[Void]() }

    fun f() -> Void {
        defer std::mem::ops::drop(() g()?)
    }
)");

// Nothing receives the value, so there must not be one.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  DeferStatementAst,
  test_invalid_defer_non_void_expression,
  SppDiscardedValueError, R"(
    fun g() -> S32 { ret 1 }

    fun f() -> Void {
        defer g()
    }
)");

// The value is taken twice: once where it is written, and again when the scope is left. Reported against the "defer",
// which is what would run on a value that is already gone.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  DeferStatementAst,
  test_invalid_value_consumed_explicitly_and_by_defer,
  SppDeferConsumesMovedValueError, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        !public
        fun close(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let h = Handle(fd=1)
        defer h.close()
        h.close()
    }
)");
