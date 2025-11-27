module;
#include <spp/macros.hpp>

export module spp.utils.strings;
import std;


namespace spp::utils::strings {
    /**
     * Check if a character is alphanumeric (a-z, A-Z, 0-9) or an underscore (_).
     * @param c The character to check.
     * @return If the character is alphanumeric or an underscore.
     */
    SPP_EXP auto is_alphanumeric(
        char c)
        -> bool;

    /**
     * Convert a string from snake_case to PascalCase.
     * @param str The snake_case string.
     * @return The PascalCase string.
     */
    SPP_EXP auto snake_to_pascal(
        std::string const &)
        -> std::string;

    /**
     * Find the closest matching string from a list of choices to a given query string.
     * @param query The query string to match.
     * @param choices The list of choice strings to compare against.
     * @return The closest matching string, or @c std::nullopt if no match is found.
     */
    SPP_EXP auto closest_match(
        std::string_view query,
        std::vector<std::string> const &choices)
        -> std::optional<std::string>;

    SPP_EXP auto levenshtein(
        std::string_view s1,
        std::string_view s2)
        -> std::size_t;

    SPP_EXP auto similarity_ratio(
        std::string_view s1,
        std::string_view s2)
        -> double;
}
