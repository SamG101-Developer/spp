#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_object_destructure, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    sup Point ext Copy { }

    fun f() -> Void {
        let p = Point(x=1, y=2)
        case p is Point(x, y) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_tuple_destructure, R"(
    fun f() -> Void {
        let t = (1, 2)
        case t is (a, b) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_array_destructure, R"(
    fun f() -> Void {
        let a = [1, 2, 3]
        case a is [x, y, z] { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_nested_variant_pattern, R"(
    fun f() -> Void {
        let o: std::option::Opt[std::option::Opt[S32]] =
            std::option::Some(val=std::option::Some(val=1))
        case o is std::option::Some[std::option::Some[S32]](val) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_subject_is_a_call_rather_than_a_name, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun make() -> Point {
        ret Point(x=1, y=2)
    }

    fun f() -> Void {
        case make() is Point(x, y) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_subject_reached_through_a_field, R"(
    cls Inner {
        !public a: S32
        !public b: S32
    }
    cls Outer { !public inner: Inner }

    sup Inner ext Copy { }
    sup Outer ext Copy { }

    fun f() -> Void {
        let o = Outer(inner=Inner(a=1, b=2))
        case o.inner is Inner(a, b) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_skip_the_remaining_attributes, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    sup Point ext Copy { }

    fun f() -> Void {
        let p = Point(x=1, y=2)
        case p is Point(x, ..) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_borrowed_subject, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun g(p: &Point) -> Void {
        case p is Point(&x, ..) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_several_branches_over_one_subject, R"(
    fun f() -> Void {
        let o: std::option::Opt[S32] = std::option::Some(val=1)
        case o of {
            is std::option::Some[S32](val) { }
            is std::option::None() { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCaseSubjectEvaluation,
  test_valid_bare_alternative_of_a_variant, R"(
    fun f() -> Void {
        let v: S32 or Bool = 1
        case v of {
            is S32() { }
            is Bool() { }
            else { }
        }
    }
)");
