module;
#include <spp/macros.hpp>

export module spp.asts.utils.visibility;
import std;

/// @cond
namespace spp::asts::utils {
    SPP_EXP enum class Visibility : std::uint8_t;
}

/// @endcond


SPP_EXP enum class spp::asts::utils::Visibility : std::uint8_t {
    PUBLIC = 0,
    PROTECTED = 1,
    PRIVATE = 2
};
