module;
#include <spp/macros.hpp>

export module spp.utils.traits;
import boost;
import std;

namespace spp::utils::traits {
  SPP_EXP_CON template <typename T>
  concept integral = std::integral<T>
    || boost::number_category<T>::value == boost::number_kind_integer;

  SPP_EXP_CON template <typename T>
  concept floating_point = std::floating_point<T>
    || boost::number_category<T>::value == boost::number_kind_floating_point;
}
