#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_invalid_multiple_multi_skip,
  SppMultipleRestPatternsError, R"(
    fun f(p: (Str, Str)) -> Void {
        case p is [.., ..] { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_invalid_missing_value,
  SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f(p: (Str, Str)) -> Void {
        case p is (x) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_invalid_extra_value,
  SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f(p: (Str, Str)) -> Void {
        case p is (x, y, z) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_value_only, R"(
    fun f(p: (Str, Str)) -> Void {
        case p is (x, y) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_value_and_single_skip, R"(
    fun f(p: (Str, Str)) -> Void {
        # Bound by borrow: the skipped element is never taken,
        # so binding by value would leave "p" owning it with
        # nothing left able to consume it.
        case p is (&x, _) { }
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_value_and_unbound_multi_skip, R"(
    fun f(p: (Str, Str)) -> Void {
        case p is (&x, ..) { }
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_value_and_bound_multi_skip, R"(
    fun f(p: (Str, Str)) -> Void {
        case p is (..x) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_value_before_and_after_bound_multi_skip, R"(
    fun f(p: (S32, Str, Str, Str, Bool)) -> Void {
        case p is (a, ..b, c) {
            let x: Bool = c
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_literal_before_and_after_bound_multi_skip, R"(
    fun f(p: (S32, S32, S32, S32, Bool)) -> Void {
        case p is (1, ..b, true) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_multiple_branches, R"(
    fun f(p: (S32, S32)) -> Void {
        case p of {
            is (1, y) { }
            is (x, z) { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureArrayAst,
  test_valid_nested_array_in_tuple, R"(
    fun f(p: (Arr[Str, 2_uz], Arr[Str, 2_uz])) -> Void {
        case p is ([a, b], [c, d]) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureArrayAst,
  test_valid_nested_tuple_in_tuple, R"(
    fun f(p: ((Str, Str), (Str, Str))) -> Void {
        case p is ((a, b), (c, d)) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureArrayAst,
  test_valid_nested_object_in_tuple, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }
    fun f(p: (Point, Point)) -> Void {
        case p is (Point(x as a, y as b), Point(x as c, y as d)) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureTupleAst,
  test_valid_float_literal_in_tuple_pattern, R"(
    fun f(t: (F64, S32)) -> S32 {
        case t of {
            is (1.5_f64, n) { ret n }
            else { ret 0 }
        }
    }
)");
