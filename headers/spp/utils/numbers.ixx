module;
#include <spp/macros.hpp>

export module spp.utils.numbers;
import spp.utils.types;
import numex.big_dec;
import numex.big_int;

namespace spp::utils::numbers {
  SPP_EXP_CLS using IntLimitMap = Map<Str, Pair<numex::BigInt, numex::BigInt>>;
  SPP_EXP_CLS using FloatLimitMap = Map<Str, Pair<numex::BigDec, numex::BigDec>>;

  /// The lowest and highest value of each integer type, by its literal suffix ("u8", "sz", ...).
  SPP_EXP_FUN auto IntegerBounds() -> IntLimitMap const&;

  /// The lowest and highest value of each float type, by its literal suffix ("f32", ...).
  SPP_EXP_FUN auto FloatBounds() -> FloatLimitMap const&;
}
