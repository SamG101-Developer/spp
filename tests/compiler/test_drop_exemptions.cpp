#include "test_macros.hpp"

// Implicit drops are only as good as the set of things they deliberately do not apply to. Each of these exemptions was
// added for a specific reason, and each one is a silent hole if it ever widens - so they are pinned here alongside a
// case that is still dropped, to catch an exemption that has started swallowing too much.

// A function whose body is not written in S++ has no body that could have consumed its parameters.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropExemptions,
    test_valid_intrinsic_parameters_not_reported, R"(
    use std::annotations::intrinsic

    cls T { }

    # Named after a real entry in "kBuiltinFuncs" - the name has to resolve to one, and its signature is the shape
    # this needs anyway. A made-up name is reported as an internal compiler error rather than a bad annotation.
    !intrinsic(name="std.mem.ops.drop")
    fun swallow[U](v: U) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropExemptions,
    test_valid_abstract_method_parameters_not_reported, R"(
    cls T { }
    cls Base { }

    sup Base {
        !abstract_method
        fun swallow(&self, v: T) -> Void { }
    }
)");

// A closure that captures by mutable borrow was once exempt: it cannot be moved away, so consuming it seemed to demand
// a move the language forbids. It owns its environment, though, and a drop ends it and its loans together (M9.3), so
// it is dropped at its scope's end like any other value. Pinned here so the old exemption does not quietly return.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropExemptions,
  test_valid_mut_capturing_closure_no_longer_exempt, R"(
    fun f() -> Void {
        let mut c = 2
        let lambda = |x: S32 caps &mut c| { x }
    }
)");

// A borrow points at a value that belongs to somebody else.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropExemptions,
    test_valid_borrow_parameters_not_reported, R"(
    cls T { }

    fun peek(a: &T, b: &mut T) -> Void { }
)");

// A "ret" part-way through a scope is reached before the "let"s below it ever run, so those symbols hold nothing yet.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropExemptions,
    test_valid_symbol_declared_after_the_exit_not_reported, R"(
    cls T { }

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(early: Bool) -> Void {
        case early { ret }
        let v = T()
        consume(v)
    }
)");

// Being zero-sized is a statement about layout, not about what an assignment means, so it no longer implies "Copy" -
// a marker type is moved, and dropped at its scope's end.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropExemptions,
  test_valid_zero_type_is_not_copy, R"(
    !zero_type
    cls Marker { }

    fun f() -> Void {
        let m = Marker
    }
)");

// A variant is copyable exactly when every alternative it can hold is.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropExemptions,
    test_valid_variant_of_copyable_alternatives_is_copy, R"(
    cls A { }
    cls B { }
    sup A ext Copy { }
    sup B ext Copy { }
    type AB = A or B

    fun f(x: AB) -> Void { }
)");

// ...and is not copyable when one of them is not, so it is moved, and dropped at its scope's end.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropExemptions,
  test_valid_variant_with_a_non_copyable_alternative, R"(
    cls A { }
    cls B { }
    sup A ext Copy { }
    type AB = A or B

    fun f(x: AB) -> Void { }
)");
