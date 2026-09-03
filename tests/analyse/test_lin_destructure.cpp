#include "../test_macros.hpp"

// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDestructure,
    test_valid_destructure_consumes_whole_value, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
        let Handle(fd) = h
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDestructure,
    test_valid_tuple_destructure_consumes, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let t = (Handle(fd=1), Handle(fd=2))
        let (a, b) = t
        let Handle(fd as fd_a) = a
        let Handle(fd as fd_b) = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDestructure,
    test_valid_fieldless_value_destructured, R"(
    cls Marker { }

    fun f() -> Void {
        let m = Marker()
        let Marker() = m
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDestructure,
    test_invalid_fieldless_value_left_live,
    SppLinearValueNotConsumedError, R"(
    cls Marker { }

    fun f() -> Void {
        let m = Marker()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDestructure,
    test_invalid_copy_attribute_read_does_not_consume,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
        let fd = h.fd
    }
)");

// A "let" destructure takes the value apart, so it needs to own it - the grammar has no place for a convention on
// one. Through a borrow the attributes are read instead, which is what leaves the borrowed value untouched.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearDestructure,
    test_valid_reading_through_a_borrow_does_not_consume, R"(
    cls Handle { !public fd: S32 }

    fun peek(h: &Handle) -> S32 {
        ret h.fd
    }
)");

// ...and taking it apart through one is refused, because that would need to own it.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearDestructure,
    test_invalid_destructure_through_a_borrow,
    SppTypeMismatchError, R"(
    cls Handle { !public fd: S32 }

    fun peek(h: &Handle) -> S32 {
        let Handle(fd) = h
        ret fd
    }
)");
