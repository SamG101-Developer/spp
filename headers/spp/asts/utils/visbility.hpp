#pragma once

#include <cstdint>

/// @cond
namespace spp::asts::utils {
    enum class Visibility : std::uint8_t;
}
/// @endcond


enum class spp::asts::utils::Visibility : std::uint8_t {
    PUBLIC = 0,
    PROTECTED = 1,
    PRIVATE = 2
};
