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
    numex::BigInt const &value, Str const &type) -> std::optional<numex::BigInt>;

  /// The "value" as a "u64", when it is one: nothing when
  /// it is negative or past the top of the range.
  SPP_EXP_FUN auto ToU64(
    numex::BigInt const &value) -> std::optional<std::uint64_t>;
}
