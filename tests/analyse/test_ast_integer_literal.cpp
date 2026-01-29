#include "../test_macros.hpp"


#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
    #define OS_BITS 64
#else
    #define OS_BITS 32
#endif


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u8_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_u8
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u8_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 256_u8
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i8_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -129_s8
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i8_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 128_s8
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u16_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_u16
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u16_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 65536_u16
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i16_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -32769_s16
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i16_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 32768_s16
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u32_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_u32
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u32_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 4294967296_u32
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i32_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -2147483649_s32
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i32_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 2147483648_s32
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u64_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_u64
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u64_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 18446744073709551616_u64
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_usize_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_uz
    }
)");


#if OS_BITS == 64
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_usize_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 18446744073709551616_uz
    }
)");
#elif OS_BITS == 32
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_usize_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 4294967296_uz
    }
)");
#endif


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i64_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -9223372036854775809_s64
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i64_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 9223372036854775808_s64
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u128_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_u128
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u128_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 340282366920938463463374607431768211456_u128
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i128_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -170141183460469231731687303715884105729_s128
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i128_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 170141183460469231731687303715884105728_s128
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u256_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -1_u256
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_u256_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 115792089237316195423570985008687907853269984665640564039457584007913129639936_u256
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i256_lower_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = -57896044618658097711785492504343953926634992332820282019728792003956564819969_s256
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IntegerLiteralAst,
    test_invalid_i256_upper_bound,
    SppIntegerOutOfBoundsError, R"(
    fun f() -> std::void::Void {
        let x = 57896044618658097711785492504343953926634992332820282019728792003956564819968_s256
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_u8, R"(
    fun f() -> std::void::Void {
        let x = 0_u8
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_i8, R"(
    fun f() -> std::void::Void {
        let x = -128_s8
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_u16, R"(
    fun f() -> std::void::Void {
        let x = 65535_u16
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_i16, R"(
    fun f() -> std::void::Void {
        let x = -32768_s16
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_u32, R"(
    fun f() -> std::void::Void {
        let x = 4294967295_u32
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_i32, R"(
    fun f() -> std::void::Void {
        let x = -2147483648_s32
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_u64, R"(
    fun f() -> std::void::Void {
        let x = 18446744073709551615_u64
    }
)");


#if OS_BITS == 64
SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_usize, R"(
    fun f() -> std::void::Void {
        let x = 18446744073709551615_uz
    }
)");
#elif OS_BITS == 32
SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_usize, R"(
    fun f() -> std::void::Void {
        let x = 4294967295_uz
    }
)");
#endif


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_i64, R"(
    fun f() -> std::void::Void {
        let x = -9223372036854775808_s64
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_u128, R"(
    fun f() -> std::void::Void {
        let x = 340282366920938463463374607431768211455_u128
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_i128, R"(
    fun f() -> std::void::Void {
        let x = -170141183460469231731687303715884105728_s128
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_u256, R"(
    fun f() -> std::void::Void {
        let x = 115792089237316195423570985008687907853269984665640564039457584007913129639935_u256
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IntegerLiteralAst,
    test_valid_i256, R"(
    fun f() -> std::void::Void {
        let x = -57896044618658097711785492504343953926634992332820282019728792003956564819968_s256
    }
)");
