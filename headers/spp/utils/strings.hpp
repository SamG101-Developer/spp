#pragma once
#include <spp/pch.hpp>


/// @cond
namespace spp::utils::strings {
    /**
     * Check if a character is alphanumeric (a-z, A-Z, 0-9) or an underscore (_).
     * @param c The character to check.
     * @return If the character is alphanumeric or an underscore.
     */
    auto is_alphanumeric(char c) -> bool;

    auto snake_to_pascal(std::string const &) -> std::string;

    auto closest_match(std::string const &query, std::vector<std::string> const &choices) -> std::optional<std::string>;
}
/// @endcond
