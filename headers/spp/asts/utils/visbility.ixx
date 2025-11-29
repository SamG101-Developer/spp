module;
#include <spp/macros.hpp>

export module spp.asts.utils.visibility;
import std;

namespace spp::asts::utils {
    SPP_EXP_CLS enum class Visibility : std::uint8_t;
}


SPP_EXP_CLS enum class spp::asts::utils::Visibility : std::uint8_t {
    PUBLIC = 0,
    PROTECTED = 1,
    PRIVATE = 2
};
