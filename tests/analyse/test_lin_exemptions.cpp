#include "../test_macros.hpp"

// The linear rule is only as good as the set of things it deliberately does not apply to. Each of these exemptions was
// added for a specific reason, and each one is a silent hole if it ever widens - so they are pinned here alongside a
// case that must still be reported, to catch an exemption that has started swallowing too much.
//
// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

// A function whose body is not written in S++ has no body that could have consumed its parameters.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearExemptions,
    test_valid_intrinsic_parameters_not_reported, R"(
    use std::annotations::intrinsic

    cls T { }

    # Named after a real entry in "kBuiltinFuncs" - the name has to resolve to one, and its signature is the shape
    # this needs anyway. A made-up name is reported as an internal compiler error rather than a bad annotation.
    !intrinsic(name="std.mem.ops.drop")
    fun swallow[U](v: U) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearExemptions,
    test_valid_abstract_method_parameters_not_reported, R"(
    use std::annotations::abstract_method

    cls T { }
    cls Base { }

    sup Base {
        !abstract_method
        fun swallow(&self, v: T) -> Void { }
    }
)");

// A closure that captures by mutable borrow cannot be moved at all - the borrow rules forbid it - so requiring it to
// be consumed would demand something the language will not allow, leaving no way to write the value.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearExemptions,
    test_valid_mut_capturing_closure_not_reported, R"(
    fun f() -> Void {
        let mut c = 2
        let lambda = (x: S32 caps &mut c) { x }
    }
)");

// A borrow points at a value that belongs to somebody else.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearExemptions,
    test_valid_borrow_parameters_not_reported, R"(
    cls T { }

    fun peek(a: &T, b: &mut T) -> Void { }
)");

// A "ret" part-way through a scope is reached before the "let"s below it ever run, so those symbols hold nothing yet.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearExemptions,
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
// a marker type still has to be accounted for.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearExemptions,
    test_invalid_zero_type_is_not_copy,
    SppLinearValueNotConsumedError, R"(
    use std::annotations::zero_type

    !zero_type
    cls Marker { }

    fun f() -> Void {
        let m = Marker()
    }
)");

// A variant is copyable exactly when every alternative it can hold is.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearExemptions,
    test_valid_variant_of_copyable_alternatives_is_copy, R"(
    cls A { }
    cls B { }
    sup A ext Copy { }
    sup B ext Copy { }
    type AB = A or B

    fun f(x: AB) -> Void { }
)");

// ...and is not copyable when one of them is not, so it still has to be consumed.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearExemptions,
    test_invalid_variant_with_a_non_copyable_alternative,
    SppLinearValueNotConsumedError, R"(
    cls A { }
    cls B { }
    sup A ext Copy { }
    type AB = A or B

    fun f(x: AB) -> Void { }
)");
