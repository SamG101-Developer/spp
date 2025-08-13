#pragma once

#include <cstdint>


namespace spp::asts::utils {
    enum class Visibility : std::uint8_t;
}


enum class spp::asts::utils::Visibility : std::uint8_t {
    PUBLIC = 0,
    PROTECTED = 1,
    PRIVATE = 2
};
