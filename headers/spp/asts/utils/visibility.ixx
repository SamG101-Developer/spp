module;
#include <spp/macros.hpp>

export module spp.asts.utils.visibility;
import std;

namespace spp::asts::utils {
    SPP_EXP_ENUM enum class Visibility : std::uint8_t;
}


SPP_EXP_ENUM enum class spp::asts::utils::Visibility : std::uint8_t {
    kPublic = 0,
    kProtected = 1,
    kPrivate = 2
};
