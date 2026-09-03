#include "../test_macros.hpp"

SPP_TEST_CMP_VALUES(
  TestNumericLimits,
  test_unsigned_max_is_the_full_width, R"(
  cmp a: U8 = std::intrinsics::max_val[U8]()
  cmp b: U16 = std::intrinsics::max_val[U16]()
)", {"a", "255_u8"}, {"b", "65535_u16"});

SPP_TEST_CMP_VALUES(
  TestNumericLimits,
  test_signed_max_is_one_bit_narrower, R"(
  cmp a: S8 = std::intrinsics::max_val[S8]()
  cmp b: S16 = std::intrinsics::max_val[S16]()
)", {"a", "127_s8"}, {"b", "32767_s16"});

SPP_TEST_CMP_VALUES(
  TestNumericLimits,
  test_unsigned_min_is_zero, R"(
  cmp a: U8 = std::intrinsics::min_val[U8]()
  cmp b: U32 = std::intrinsics::min_val[U32]()
)", {"a", "0_u8"}, {"b", "0_u32"});

SPP_TEST_CMP_VALUES(
  TestNumericLimits,
  test_limits_constants_agree_with_the_intrinsics, R"(
  cmp a: U8 = std::limits::Limits[U8]::max
  cmp b: U8 = std::limits::Limits[U8]::min
  cmp c: S8 = std::limits::Limits[S8]::max
)", {"a", "255_u8"}, {"b", "0_u8"}, {"c", "127_s8"});

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNumericLimits,
  test_valid_wrapping_arithmetic_at_every_width, R"(
    fun f() -> Void {
        let a = 0_u8.sub_wrapping(1_u8)
        let b = 0_u16.sub_wrapping(1_u16)
        let c = 0_u32.sub_wrapping(1_u32)
        let d = 0_u64.sub_wrapping(1_u64)
        let e = 0_s32.sub_wrapping(1_s32)
        let g = 0_u64.add_wrapping(1_u64)
        let h = 2_u64.mul_wrapping(3_u64)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNumericLimits,
  test_valid_float_pow_on_both_overloads, R"(
    fun f() -> Void {
        let a = 2.0_f64.pow(3)
        let b = 2.0_f64.pow(3.0_f64)
        let c = 2.0_f32.pow(2)
    }
)");
