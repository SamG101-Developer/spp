module;
#include <spp/macros.hpp>

export module spp.asts.utils.visibility;
import std;

namespace spp::asts::utils {
    SPP_EXP_CLS enum class Visibility : std::uint8_t;
}

SPP_EXP_CLS enum class spp::asts::utils::Visibility : std::uint8_t {
    kPublic = 0, // Public to any module in any package.
    kPackage = 1, // Public to any module in this package.
    kProtected = 2, // Public to this module and child modules.
    kPrivate = 3 // Public to this module only.
};
