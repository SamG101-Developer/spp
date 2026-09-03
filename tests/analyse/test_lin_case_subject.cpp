#include "../test_macros.hpp"

// A "case ... of" takes its subject exactly when one of its patterns binds a value out of it, decided once for the
// whole case rather than per branch. Getting this wrong in either direction is silent: too eager and a value that was
// only tested reads as gone, too lazy and "Opt::unwrap" cannot be written at all.
//
// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

// A pattern that binds takes the subject, so it is not still owed at the end of the scope.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearCaseSubject,
    test_valid_binding_pattern_consumes_the_subject, R"(
    cls T { }
    cls A { !public v: T }
    cls B { }
    type AB = A or B

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(x: AB) -> Void {
        case x of {
            is A(v) { consume(v) }
            else { }
        }
    }
)");

// The subject is taken at the "case", before the branches, so a branch that leaves early has not abandoned it.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearCaseSubject,
    test_valid_return_inside_a_consuming_branch, R"(
    cls T { }
    cls A { !public v: T }
    cls B { }
    type AB = A or B

    fun consume(t: T) -> Void {
        let T() = t
    }

    fun f(x: AB) -> Void {
        case x of {
            is A(v) {
                consume(v)
                ret
            }
            else { }
        }
    }
)");

// A case whose patterns only test does not take its subject, so the value is still there afterwards - and still owed.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearCaseSubject,
    test_invalid_testing_pattern_leaves_the_subject_owed,
    SppLinearValueNotConsumedError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> Void {
        case p of {
            is Point(x=0, y=0) { }
            else { }
        }
    }
)");

// ...and because it was not taken, it is still usable after the case.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearCaseSubject,
    test_valid_subject_still_usable_after_a_testing_case, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun consume(p: Point) -> Void {
        let Point(x, y) = p
    }

    fun f(p: Point) -> Void {
        case p of {
            is Point(x=0, y=0) { }
            else { }
        }
        consume(p)
    }
)");

// A copyable subject is never taken, however its patterns bind - copying leaves the original in place.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearCaseSubject,
    test_valid_copyable_subject_is_not_consumed, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }
    sup Point ext Copy { }

    fun f(p: Point) -> Void {
        case p of {
            is Point(x, y) { }
            else { }
        }
        case p of {
            is Point(x, y) { }
            else { }
        }
    }
)");

// A binary "is" is a test that can be re-evaluated - a loop condition runs it every iteration - so it never consumes
// what it tests, even though it desugars to a "case" with a binding pattern. Bound by borrow, because it matches on
// one path only: binding by value would partially move the subject there and leave it whole on the other.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearCaseSubject,
    test_valid_is_expression_does_not_consume, R"(
    cls T { }
    cls A { !public v: T }
    cls B { }
    type AB = A or B

    fun consume(x: AB) -> Void {
        case x of {
            is A(v) { let T() = v }
            else { }
        }
    }

    fun f(x: AB) -> Void {
        let mut i = 0
        loop x is A(&v) {
            i += 1
            exit
        }
        consume(x)
    }
)");
