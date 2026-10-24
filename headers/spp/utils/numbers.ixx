module;
#include <spp/macros.hpp>

export module spp.utils.numbers;
import spp.utils.types;
import llvm;
import std;
import numex.big_dec;
import numex.big_int;

namespace spp::utils::numbers {
  /// The integer limit map is a map of a string (integer
  /// literal suffix) to the lower and upper bounds.
  SPP_EXP_CLS using IntLimitMap = Map<
    Str, Pair<numex::BigInt, numex::BigInt>>;

  /// The float limit map is a map of a string (float
  /// literal suffix) to the lower and upper bounds.
  SPP_EXP_CLS using FloatLimitMap = Map<
    Str, Pair<numex::BigDec, numex::BigDec>>;

  /// The lowest and highest value of each integer type, by
  /// its literal suffix ("u8", "sz", ...).
  SPP_EXP_FUN auto IntegerBounds() -> IntLimitMap const&;

  /// The lowest and highest value of each float type, by
  /// its literal suffix ("f32", ...).
  SPP_EXP_FUN auto FloatBounds() -> FloatLimitMap const&;

  /// The "value" within the range of the integer "type"
  /// (its literal suffix), as the same bits read back.
  /// Nothing for an unknown type.
  SPP_EXP_FUN auto WrapToInteger(
    numex::BigInt const &value, Str const &type) -> Opt<numex::BigInt>;

  /// The "value" shifted left by "count" as the bits of the
  /// integer "type" (its literal suffix): the bits shifted past
  /// the top are lost, as the run-time ones are.
  /// Nothing for an unknown type.
  SPP_EXP_FUN auto ShiftLeftLogical(
    numex::BigInt const &value, std::uint32_t count, Str const &type) -> Opt<numex::BigInt>;

  /// The "value" shifted right by "count" as the bits of the
  /// integer "type" (its literal suffix): a logical shift, zeros
  /// in at the top whatever the sign, as the run-time one is.
  /// Nothing for an unknown type.
  SPP_EXP_FUN auto ShiftRightLogical(
    numex::BigInt const &value, std::uint32_t count, Str const &type) -> Opt<numex::BigInt>;

  /// The format of the float "type" (its literal suffix): IEEE
  /// binary16, 32, 64 and 128, and "f8" as FP8 E4M3 (no infinity,
  /// largest 448). Comp-time float arithmetic is done in it, so
  /// it rounds as at run time.
  SPP_EXP_FUN auto FloatSemantics(
    Str const &type) -> llvm::fltSemantics const&;

  /// The decimal places an exact value of the float "type" can
  /// need: those of its smallest subnormal.
  SPP_EXP_FUN auto FloatDecimalPlaces(
    Str const &type) -> std::uint64_t;

  /// "text" ("[-]digits[.digits]") as the nearest value of the
  /// float "type", rounded as the literal is at run time.
  SPP_EXP_FUN auto ReadFloat(
    StrView text, Str const &type) -> llvm::APFloat;

  /// "ReadFloat" of a value given exactly.
  SPP_EXP_FUN auto ReadFloat(
    numex::BigDec const &value, Str const &type) -> llvm::APFloat;

  /// "value" as a decimal "[-]digits[.digits]" that reads back to
  /// it ("-0" for negative zero). Nothing when it is not finite.
  SPP_EXP_FUN auto WriteFloat(
    llvm::APFloat const &value) -> Opt<Str>;

  /// The "value" as a "u64", when it is one: nothing when
  /// it is negative or past the top of the range.
  SPP_EXP_FUN auto ToU64(
    numex::BigInt const &value) -> Opt<std::uint64_t>;
}
