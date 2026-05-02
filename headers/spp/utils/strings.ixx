module;
#include <spp/macros.hpp>

export module spp.utils.strings;
import spp.utils.types;
import std;
import mppp;

namespace spp::utils::strings {
    /**
     * Check if a character is alphanumeric (a-z, A-Z, 0-9) or an underscore (_).
     * @param c The character to check.
     * @return If the character is alphanumeric or an underscore.
     */
    SPP_EXP_FUN auto IsAlNum(
        char c)
        -> bool;

    /**
     * Convert a string from snake_case to PascalCase.
     * @param str The snake_case string.
     * @return The PascalCase string.
     */
    SPP_EXP_FUN auto SnakeToPascal(
        Str const &)
        -> Str;

    /**
     * Find the closest matching string from a list of choices to a given query string.
     * @param query The query string to match.
     * @param choices The list of choice strings to compare against.
     * @return The closest matching string, or @c std::nullopt if no match is found.
     */
    SPP_EXP_FUN auto ClosestMatch(
        StrView query,
        Vec<Str> const &choices)
        -> std::optional<Str>;

    auto Levenshtein(
        StrView s1,
        StrView s2)
        -> std::size_t;

    auto SimilarityRatio(
        StrView s1,
        StrView s2)
        -> double;

    /**
     * Convert 0b binary, 0x hex, 0o oct, or decimal into decimal, and remove any underscores. This is used to normalize
     * integer literals for comparison to their bounds.
     * @param s1 The string to normalize. This should be the raw string from the integer literal token, without the sign
     * or type suffix.
     * @return The normalized string, which is the decimal representation of the integer literal with no underscores.
     */
    SPP_EXP_FUN auto NormaliseIntegerString(
        StrView s1)
        -> mppp::BigInt;

    /**
     * Convert two base 10 strings into a big float, and remove any underscores. This is used to normalize float
     * literals for comparison to their bounds.
     * @param s1 The integer part of the float literal. This should be the raw string from the integer part of the float
     * literal token, without the sign or dot.
     * @param s2 The fractional part of the float literal. This should be the raw string from the fractional part of the
     * float literal token, without the dot or type suffix.
     * @return The normalized string, which is the decimal representation of the float literal with no underscores, in
     * the form "integer_part.fractional_part".
     */
    SPP_EXP_FUN auto NormalizeFloatString(
        StrView s1,
        StrView s2)
        -> mppp::BigDec;

    /**
     * Convert a numeric string that includes e+ or e- into a normal base 10 string, and remove any underscores.
     * @param s1 The string to normalize. This should be the raw string from the float literal token, without the sign
     * or type suffix.
     * @return The normalized string, which is the decimal representation of the float literal with no underscores, in
     * the form "integer_part".
     */
    SPP_EXP_FUN auto ExpandScientificNotation(
        StrView s1)
        -> mppp::BigDec;
}
