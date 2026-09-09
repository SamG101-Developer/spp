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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestLinearDestructure,
  test_valid_reading_through_a_borrow_does_not_consume, R"(
    cls Handle { !public fd: S32 }

    fun peek(h: &Handle) -> S32 {
        ret h.fd
    }
)");

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

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearDestructure,
  test_invalid_destructure_skips_an_owned_attribute,
  SppDestructureSkipsOwnedPartError, R"(
    cls Point { !public x: Str
                !public y: Str }

    fun f(p: Point) -> Void {
        let Point(x, ..) = p
        drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearDestructure,
  test_invalid_tuple_destructure_skips_an_owned_element,
  SppDestructureSkipsOwnedPartError, R"(
    fun f(t: (Str, Str)) -> Void {
        let (a, ..) = t
        drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearDestructure,
  test_invalid_case_branch_skips_an_owned_attribute,
  SppDestructureSkipsOwnedPartError, R"(
    cls Point { !public x: Str
                !public y: Str }

    fun f(p: Point) -> Void {
        case p of {
            is Point(x, ..) { drop(x) }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestLinearDestructure,
  test_valid_destructure_skips_a_copyable_attribute, R"(
    cls Point { !public x: Str
                !public y: U32 }

    fun f(p: Point) -> Void {
        let Point(x, ..) = p
        drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestLinearDestructure,
  test_valid_shape_test_takes_nothing_apart, R"(
    cls Point { !public x: Str
                !public y: Str }

    fun f(p: Point) -> Void {
        let b = p is Point(..)
        drop(b)
        drop(p)
    }
)");
