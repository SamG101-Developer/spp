#include <spp/lex/tokens.hpp>


spp::lex::RawToken::RawToken(const RawTokenType type, icu::UnicodeString &&data)
    : type(type), data(std::move(data)) {
}
