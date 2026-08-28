#include "../test_macros.hpp"

// This file used to test compiler-inserted destruction: "Del::del" run at scope exit, drop flags for conditionally
// moved values, unwind drops at a loop "exit", and the destruction of a discarded statement value. None of that
// exists any more - ownership is linear, so nothing is destroyed implicitly and every one of those cases is now a
// compile time error instead. What survives is "Drop", an ordinary trait whose "drop" takes "self" by move and is
// called explicitly, so that is what is tested here.
//
// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstDestructors,
    test_valid_drop_called_explicitly, R"(
    cls Handle { !public fd: S32 }

    sup Handle ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let h = Handle(fd=1)
        h.drop()
    }
)");

// "drop" is found through a superimposition chain like any other method, so a type that inherits its destructor is
// destroyed by the one it inherits.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstDestructors,
    test_valid_drop_inherited_through_a_chain, R"(
    cls Base { !public fd: S32 }
    cls Derived { }

    sup Base ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Self(fd) = self
        }
    }

    sup Derived ext Base { }

    fun f() -> Void {
        let d = Derived(fd=1)
        d.drop()
    }
)");

// Every copyable type is discardable, which is what lets a generic constrained by "Drop" take a number.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstDestructors,
    test_valid_copyable_type_is_droppable, R"(
    fun discard[T: std::ops::drop::Drop](v: T) -> Void {
        v.drop()
    }

    fun f() -> Void {
        discard(123)
    }
)");

// Nothing is destroyed implicitly any more, so a value left holding something at the end of its scope is an error
// rather than something the compiler quietly cleans up. This is the case the old first test asserted the opposite of.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstDestructors,
    test_invalid_value_not_dropped_at_scope_exit,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    sup Handle ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let h = Handle(fd=1)
    }
)");

// "drop" consumes, so it cannot be called twice - which is what makes a double release unrepresentable rather than
// something a drop flag has to prevent at runtime.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstDestructors,
    test_invalid_drop_called_twice,
    SppUninitializedMemoryUseError, R"(
    cls Handle { !public fd: S32 }

    sup Handle ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Handle(fd) = self
        }
    }

    fun f() -> Void {
        let h = Handle(fd=1)
        h.drop()
        h.drop()
    }
)");

// A type with no "Drop" has no "drop" to call; it is taken apart instead.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstDestructors,
    test_invalid_drop_on_type_without_drop,
    SppIdentifierUnknownError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
        h.drop()
    }
)");
