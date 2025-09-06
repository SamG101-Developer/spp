#ifndef STRINGS_HPP
#define STRINGS_HPP

#include <optional>
#include <string>
#include <vector>


namespace spp::utils::strings {
    auto is_alphanumeric(char c) -> bool;

    auto snake_to_pascal(std::string const &) -> std::string;

    auto closest_match(std::string const &query, std::vector<std::string> const &choices) -> std::optional<std::string>;
}


#endif //STRINGS_HPP
