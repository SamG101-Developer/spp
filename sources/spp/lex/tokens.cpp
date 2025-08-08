#include <spp/lex/tokens.hpp>


spp::lex::RawToken::RawToken(const RawTokenType type, std::string data) :
    type(type),
    data(std::move(data)) {
}
