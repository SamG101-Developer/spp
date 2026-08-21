#include "../test_macros.hpp"

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_literal, R"(
  cmp a: S32 = 42
  cmp b: U64 = 7_u64
)", {"a", "42_s32"}, {"b", "7_u64"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_float_literal, R"(
  cmp a: F32 = 1.5
  cmp b: F64 = 2.25_f64
)", {"a", "1.5_f32"}, {"b", "2.25_f64"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_byte_and_string_literal, R"(
  cmp a: U8 = b'x'
  cmp b: &StrView = "hi"
)", {"a", "b'x'"}, {"b", "\"hi\""});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_arithmetic, R"(
  cmp a: S32 = 6 * 7
  cmp b: S32 = 10 - 4
)", {"a", "42_s32"}, {"b", "6_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_operator_precedence, R"(
  cmp a: S32 = 2 + 3 * 4
  cmp b: S32 = (2 + 3) * 4
)", {"a", "14_s32"}, {"b", "20_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_boolean_expression, R"(
  cmp a: Bool = 1 < 2
  cmp b: Bool = 2 < 1
  cmp c: Bool = 1 < 2 and 3 > 4
)", {"a", "true"}, {"b", "false"}, {"c", "false"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_keyword_not_postfix, R"(
  cmp a: Bool = (1 < 2).not
  cmp b: Bool = (1 > 2).not
)", {"a", "false"}, {"b", "true"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_array_literal, R"(
  cmp a: Arr[S32, 3_uz] = [1, 2, 3]
)", {"a", "[1_s32, 2_s32, 3_s32, ]"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_tuple_literal_and_element_access, R"(
  cmp a: (S32, Bool) = (7, true)
  cmp b: S32 = a.0
  cmp c: Bool = a.1
)", {"a", "(7_s32, true, )"}, {"b", "7_s32"}, {"c", "true"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_object_initializer_and_attribute_access, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }

  cmp p: Point = Point(x=3, y=4)
  cmp a: S32 = p.x
  cmp b: S32 = p.y
  cmp c: S32 = Point(x=1, y=2).y
)", {"a", "3_s32"}, {"b", "4_s32"}, {"c", "2_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_chain_of_references, R"(
  cmp a: S32 = 1
  cmp b: S32 = a + 1
  cmp c: S32 = b + a
  cmp d: S32 = c * b
)", {"a", "1_s32"}, {"b", "2_s32"}, {"c", "3_s32"}, {"d", "6_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_composite_referencing_another_constant, R"(
  cmp a: S32 = 7
  cmp arr: Arr[S32, 2_uz] = [a, a]
  cmp tup: (S32, S32) = (a, 2)
)", {"arr", "[7_s32, 7_s32, ]"}, {"tup", "(7_s32, 2_s32, )"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_function_call, R"(
  cmp fun double(x: S32) -> S32 { ret x * 2 }
  cmp a: S32 = double(21)
)", {"a", "42_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_function_call_keyword_arguments, R"(
  cmp fun sub(a: S32, b: S32) -> S32 { ret a - b }
  cmp in_order: S32 = sub(a=10, b=1)
  cmp reordered: S32 = sub(b=1, a=10)
)", {"in_order", "9_s32"}, {"reordered", "9_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_function_call_default_parameter, R"(
  cmp fun add(a: S32, b: S32 = 5) -> S32 { ret a + b }
  cmp defaulted: S32 = add(1)
  cmp given: S32 = add(1, 100)
)", {"defaulted", "6_s32"}, {"given", "101_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_let_statements, R"(
  cmp fun f(n: S32) -> S32 {
    let a = n + 1
    let b = a * 2
    ret b
  }

  cmp a: S32 = f(3)
)", {"a", "8_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_case_boolean_form_with_return, R"(
  cmp fun f(n: S32) -> S32 {
    case n < 5 { ret 111 }
    ret 222
  }

  cmp taken: S32 = f(1)
  cmp not_taken: S32 = f(9)
)", {"taken", "111_s32"}, {"not_taken", "222_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_case_of_literal_patterns, R"(
  cmp fun pick(n: S32) -> S32 {
    ret case n of {
      == 1 { 10 }
      == 2 { 20 }
      else { 30 }
    }
  }

  cmp first: S32 = pick(1)
  cmp second: S32 = pick(2)
  cmp fallback: S32 = pick(9)
)", {"first", "10_s32"}, {"second", "20_s32"}, {"fallback", "30_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_case_of_tuple_destructure, R"(
  cmp fun pick(t: (S32, S32)) -> S32 {
    ret case t of {
      is (1, y) { y }
      else { 0 }
    }
  }

  cmp matched: S32 = pick((1, 5))
  cmp unmatched: S32 = pick((2, 5))
)", {"matched", "5_s32"}, {"unmatched", "0_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_let_tuple_destructure, R"(
  cmp fun f(t: (S32, S32)) -> S32 {
    let (a, b) = t
    ret a + b
  }

  cmp a: S32 = f((3, 4))
)", {"a", "7_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_let_object_destructure, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }

  cmp fun f(p: Point) -> S32 {
    let Point(x, y) = p
    ret x * y
  }

  cmp a: S32 = f(Point(x=3, y=4))
)", {"a", "12_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_let_array_destructure, R"(
  cmp fun f(a: Arr[S32, 3_uz]) -> S32 {
    let [p, q, r] = a
    ret p + q + r
  }

  cmp a: S32 = f([1, 2, 3])
)", {"a", "6_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_case_of_object_destructure, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }

  cmp fun f(p: Point) -> S32 {
    ret case p of {
      is Point(x=1, y) { y }
      is Point(x, y) { x + y }
      else { 0 }
    }
  }

  cmp matched: S32 = f(Point(x=1, y=5))
  cmp fallthrough: S32 = f(Point(x=2, y=5))
)", {"matched", "5_s32"}, {"fallthrough", "7_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_case_of_array_destructure, R"(
  cmp fun f(a: Arr[S32, 3_uz]) -> S32 {
    ret case a of {
      is [1, b, c] { b + c }
      is [p, q, r] { p * q * r }
      else { 0 }
    }
  }

  cmp matched: S32 = f([1, 2, 3])
  cmp fallthrough: S32 = f([2, 3, 4])
)", {"matched", "5_s32"}, {"fallthrough", "24_s32"});

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_loop_in_comptime_function,
  SppInvalidComptimeOperationError, R"(
  cmp fun f(n: S32) -> S32 {
    let mut acc = 0
    let mut i = 0
    loop i < n {
      acc = acc + i
      i = i + 1
    }
    ret acc
  }

  cmp a: S32 = f(5)
)");

// Indexing resolves through the "index_ref" coroutine, which is not a comp-time operation.
// Todo: This needs to change, because ofc it is obvious it can work.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_array_index_in_comptime,
  SppCompileTimeConstantError, R"(
  cmp arr: Arr[S32, 3_uz] = [10, 20, 30]
  cmp a: S32 = arr[1_uz]@
)");

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_recursive_function_call, R"(
  cmp fun fib(n: U64) -> U64 {
    case n < 2_u64 { ret n }
    ret fib(n - 1_u64) + fib(n - 2_u64)
  }

  cmp base: U64 = fib(1_u64)
  cmp small: U64 = fib(3_u64)
  cmp a: U64 = fib(10_u64)
)", {"base", "1_u64"}, {"small", "2_u64"}, {"a", "55_u64"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_recursive_function_call_with_locals, R"(
  cmp fun sum_to(n: S32) -> S32 {
    let doubled = n * 2
    case n < 1 { ret 0 }
    ret sum_to(n - 1) + doubled
  }

  cmp a: S32 = sum_to(4)
)", {"a", "20_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_recursive_fibonacci_accumulator, R"(
  cmp fun fib_acc(n: U64, a: U64, b: U64) -> U64 {
    case n == 0_u64 { ret a }
    ret fib_acc(n - 1_u64, b, a + b)
  }

  cmp fib50: U64 = fib_acc(50_u64, 0_u64, 1_u64)
  cmp fib90: U64 = fib_acc(90_u64, 0_u64, 1_u64)
)", {"fib50", "12586269025_u64"}, {"fib90", "2880067194370816120_u64"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_case_expression_as_module_level_value, R"(
  cmp a: S32 = case 1 of {
    == 1 { 10 }
    else { 30 }
  }
)", {"a", "10_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_variadic_parameter_elements, R"(
  cmp fun sum3(..xs: S32) -> S32 { ret xs.0 + xs.1 + xs.2 }
  cmp a: S32 = sum3(1, 2, 3)
)", {"a", "6_s32"});

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_overflow_from_addition,
  SppIntegerOutOfBoundsError, R"(
  cmp x: S32 = 2000000000
  cmp y: S32 = 2000000000
  cmp z: S32 = x + y
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_overflow_from_addition_unsigned,
  SppIntegerOutOfBoundsError, R"(
  cmp a: U8 = 200_u8 + 100_u8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_overflow_from_addition_signed_upper_bound,
  SppIntegerOutOfBoundsError, R"(
  cmp a: S8 = 127_s8 + 1_s8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_underflow_from_subtraction_unsigned,
  SppIntegerOutOfBoundsError, R"(
  cmp a: U8 = 0_u8 - 1_u8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_overflow_from_multiplication,
  SppIntegerOutOfBoundsError, R"(
  cmp a: S8 = 100_s8 * 2_s8
)");

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_negative_arithmetic_results, R"(
  cmp a: S32 = 5 - 7
  cmp b: S32 = 3 * 0 - 4
  cmp c: S32 = 0 - 2147483648
)", {"a", "-2_s32"}, {"b", "-4_s32"}, {"c", "-2147483648_s32"});

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_overflow_from_addition,
  SppIntegerOutOfBoundsError, R"(
  cmp x: S32 = 2000000000
  cmp y: S32 = 2000000000
  cmp z: S32 = x + y
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_overflow_unsigned,
  SppIntegerOutOfBoundsError, R"(
  cmp a: U8 = 200_u8 + 100_u8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_overflow_signed_upper_bound,
  SppIntegerOutOfBoundsError, R"(
  cmp a: S8 = 127_s8 + 1_s8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_underflow_unsigned,
  SppIntegerOutOfBoundsError, R"(
  cmp a: U8 = 0_u8 - 1_u8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_overflow_from_multiplication,
  SppIntegerOutOfBoundsError, R"(
  cmp a: S8 = 100_s8 * 2_s8
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_float_overflow_from_addition,
  SppFloatOutOfBoundsError, R"(
  cmp a: F32 = 300000000000000000000000000000000000000.0 + 300000000000000000000000000000000000000.0
)");

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_negative_integer_results, R"(
  cmp a: S32 = 5 - 7
  cmp b: S32 = 3 * 0 - 4
  cmp c: S32 = 0 - 7 / 2
)", {"a", "-2_s32"}, {"b", "-4_s32"}, {"c", "-3_s32"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_float_arithmetic, R"(
  cmp a: F32 = 1.5 + 2.25
  cmp b: F32 = 1.5 - 2.25
  cmp c: F32 = 2.0 * 3.5
  cmp d: F64 = 1.0_f64 / 4.0_f64
)", {"a", "3.75_f32"}, {"b", "-0.75_f32"}, {"c", "7.0_f32"}, {"d", "0.25_f64"});

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_integer_division_and_bitwise, R"(
  cmp band: S32 = 12 & 10
  cmp bior: S32 = 12 | 10
  cmp bxor: S32 = 12 ^ 10
  cmp div: S32 = 7 / 2
  cmp rem: S32 = 7 % 2
)", {"band", "8_s32"}, {"bior", "14_s32"}, {"bxor", "6_s32"}, {"div", "3_s32"}, {"rem", "1_s32"});

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_division_by_zero,
  SppDivisionByZeroError, R"(
  cmp a: S32 = 7 / 0
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_integer_remainder_by_zero,
  SppDivisionByZeroError, R"(
  cmp a: S32 = 7 % 0
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_float_division_by_zero,
  SppDivisionByZeroError, R"(
  cmp a: F32 = 7.0 / 0.0
)");

// The divisor being a constant rather than a written zero makes no difference - it is resolved by the time the
// operation is reached.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_division_by_zero_constant,
  SppDivisionByZeroError, R"(
  cmp d: S32 = 0
  cmp a: S32 = 7 / d
)");

// Dividing the most negative value by -1 has no representable result, which the bounds check already covers.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_division_overflow,
  SppIntegerOutOfBoundsError, R"(
  cmp m: S32 = 0 - 2147483648
  cmp a: S32 = m / (0 - 1)
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_shift_left_by_type_width,
  SppShiftAmountOutOfBoundsError, R"(
  cmp a: S32 = 1 << 32_u32
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_shift_left_beyond_type_width,
  SppShiftAmountOutOfBoundsError, R"(
  cmp a: S32 = 1 << 100_u32
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCompTimeValues,
  test_invalid_shift_right_beyond_type_width,
  SppShiftAmountOutOfBoundsError, R"(
  cmp a: S32 = 256 >> 40_u32
)");

SPP_TEST_CMP_VALUES(
  TestCompTimeValues,
  test_shifts_within_type_width, R"(
  cmp a: S32 = 1 << 4_u32
  cmp b: S32 = 1 << 31_u32
  cmp c: S32 = 256 >> 4_u32
)", {"a", "16_s32"}, {"b", "-2147483648_s32"}, {"c", "16_s32"});
