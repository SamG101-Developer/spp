#include "test_macros.hpp"

// "bit_cast" is bounded to "BitCastable" (the sized numbers, through "Num", and "F8"): a type with fields or an
// invariant is rejected where it is written. A struct-to-struct cast was a compiler crash ("Invalid bitcast").

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestBitCast,
    test_invalid_struct_to_struct,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { !public x: U64 }
    cls B { !public y: U64 }

    fun f() -> Void {
        let b = std::intrinsics::bit_cast[B](A(x=7_u64))
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestBitCast,
    test_invalid_integer_to_str,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let s = std::intrinsics::bit_cast[Str](4096_u64)
        std::mem::ops::drop(s)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestBitCast,
    test_invalid_integer_to_char,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let c = std::intrinsics::bit_cast[Char](55296_u32)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestBitCast,
    test_valid_between_numbers_and_f8, R"(
    fun f() -> Void {
        let x = std::intrinsics::bit_cast[F64](4609434218613702656_u64)
        let y = std::intrinsics::bit_cast[S32](4294967295_u32)
        let e = std::intrinsics::bit_cast[F8](60_u8)
        let b = std::intrinsics::bit_cast[U8](e)
    }
)");
