module;
#include <spp/macros.hpp>

export module spp.asts.utils.visibility;
import std;

namespace spp::asts::utils {
  SPP_EXP_CLS enum class Visibility;
}

SPP_EXP_CLS enum class spp::asts::utils::Visibility {
  kPublic = 1, // Public to any module in any package.
  kPackage = 2, // Public to any module in this package.
  kProtected = 3, // Public to this module and child modules.
  kPrivate = 4 // Public to this module only.
};
