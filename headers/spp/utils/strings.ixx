module;
#include <spp/macros.hpp>

export module spp.utils.strings;
import spp.utils.traits;
import spp.utils.types;
import std;

namespace std {
  SPP_EXP_FUN template <typename T> requires spp::utils::traits::floating_point<T>
  auto to_string(const T value) -> std::string {
    std::array<char, 128> buf;
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
    return std::string(buf.data(), ptr);
  }
}

namespace spp::utils::strings {
  /// Check if a character is alphanumeric (a-z, A-Z, 0-9) or
  /// an underscore.
  SPP_EXP_FUN auto IsAlNum(char c) -> bool;

  /// Convert a string from snake_case to PascalCase.
  SPP_EXP_FUN auto SnakeToPascal(Str const &) -> Str;

  /// Find the closest match to "query" among "choices", or
  /// "std::nullopt" if nothing matches.
  SPP_EXP_FUN auto ClosestMatch(StrView query, Vec<Str> const &choices) -> std::optional<Str>;

  auto Levenshtein(StrView s1, StrView s2) -> std::size_t;

  auto SimilarityRatio(StrView s1, StrView s2) -> double;

  /// Convert a 0b binary, 0x hex, 0o octal or decimal integer
  /// string into decimal, removing any underscores, so integer
  /// literals can be compared against their bounds. Takes the
  /// raw token text without the sign or type suffix.
  SPP_EXP_FUN auto NormaliseIntegerString(StrView s1) -> Str;

  /// Join the parts of a float literal into one base 10 string
  /// of the form "integer_part.fractional_part", removing any
  /// underscores, so float literals can be compared against
  /// their bounds. "s1" is the integer part (no sign or dot),
  /// "s2" the fractional part (no dot or type suffix), and
  /// "exp" the exponent (no "e" or type suffix), or empty when
  /// there is none.
  SPP_EXP_FUN auto NormalizeFloatString(StrView s1, StrView s2, StrView exp = "") -> Str;

  /// Strip every underscore from a numeric string, so the boost
  /// constructors can be called cleanly on it, for integers or
  /// rationals.
  SPP_EXP_FUN auto NormaliseAnyString(StrView s1) -> Str;

  /// Decode a single-quoted char literal token (quotes
  /// included, e.g. "'a'") into its Unicode code point.
  /// Handles escape sequences (e.g. "\n") and multi-byte UTF-8
  /// scalar values.
  SPP_EXP_FUN auto DecodeCharLiteral(StrView token_data) -> std::uint32_t;

  /// Decode a double-quoted string literal token (quotes
  /// included, e.g. "\"ab\"") into its raw bytes, resolving
  /// escape sequences (e.g. "\n"). Multi-byte UTF-8 bytes pass
  /// through unchanged.
  SPP_EXP_FUN auto DecodeStringLiteral(StrView token_data) -> Str;
}
