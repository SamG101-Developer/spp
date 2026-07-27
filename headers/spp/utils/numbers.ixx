module;
#include <spp/macros.hpp>

export module spp.utils.numbers;
import spp.utils.types;
import ankerl;
import boost;

namespace spp::utils::numbers {
  SPP_EXP_CLS using IntLimitMap = ankerl::unordered_dense::map<
    Str,
    Pair<boost::BigInt, boost::BigInt>>;

  SPP_EXP_CLS using FloatLimitMap = ankerl::unordered_dense::map<
    Str,
    Pair<boost::BigDec, boost::BigDec>>;
}
