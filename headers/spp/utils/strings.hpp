#pragma once
#include <spp/pch.hpp>


namespace spp::utils::strings {
    auto is_alphanumeric(char c) -> bool;

    auto snake_to_pascal(std::string const &) -> std::string;

    auto closest_match(std::string const &query, std::vector<std::string> const &choices) -> std::optional<std::string>;
}
