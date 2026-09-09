module;
#include <spp/macros.hpp>

export module spp.utils.traits;
import std;

namespace spp::utils::traits {
  SPP_EXP_CON template <typename T>
  concept integral = std::integral<T>;

  SPP_EXP_CON template <typename T>
  concept floating_point = std::floating_point<T>;
}
