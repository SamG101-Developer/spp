module;
#include <spp/macros.hpp>

export module spp.utils.numbers;
import spp.utils.types;
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

  /// The "value" shifted right by "count" as the bits of the
  /// integer "type" (its literal suffix): a logical shift, zeros
  /// in at the top whatever the sign, as the run-time one is.
  /// Nothing for an unknown type.
  SPP_EXP_FUN auto ShiftRightLogical(
    numex::BigInt const &value, std::uint32_t count, Str const &type) -> Opt<numex::BigInt>;

  /// Whether comp-time arithmetic on floats of "type" (a literal
  /// suffix) is done on IEEE values, as at run time: "f32" and
  /// "f64", which have a host format of their own. The other
  /// widths are folded exactly.
  SPP_EXP_FUN auto IsIeeeFloat(
    Str const &type) -> bool;

  /// "text" ("[-]digits.digits") as the nearest value of the IEEE
  /// float "type", held in a "double" (an "f32" widens exactly):
  /// rounded as the literal is at run time.
  SPP_EXP_FUN auto ReadIeeeFloat(
    StrView text, Str const &type) -> double;

  /// "ReadIeeeFloat" of a value given exactly.
  SPP_EXP_FUN auto ReadIeeeFloat(
    numex::BigDec const &value, Str const &type) -> double;

  /// "value" rounded to the IEEE float "type", written as the
  /// shortest decimal that reads back to it ("-0" for negative
  /// zero). Nothing when it is not finite.
  SPP_EXP_FUN auto WriteIeeeFloat(
    double value, Str const &type) -> Opt<Str>;

  /// The "value" as a "u64", when it is one: nothing when
  /// it is negative or past the top of the range.
  SPP_EXP_FUN auto ToU64(
    numex::BigInt const &value) -> Opt<std::uint64_t>;
}
