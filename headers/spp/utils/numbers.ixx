module;
#include <spp/macros.hpp>

export module spp.utils.numbers;
import spp.utils.types;
import boost;

namespace spp::utils::numbers {
  SPP_EXP_CLS using IntLimitMap = Map<Str, Pair<boost::BigInt, boost::BigInt>>;
  SPP_EXP_CLS using FloatLimitMap = Map<Str, Pair<boost::BigDec, boost::BigDec>>;
}
