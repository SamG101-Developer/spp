module;
#include <spp/macros.hpp>

module spp.lex.tokens;


SPP_MOD_BEGIN
spp::lex::RawToken::RawToken(const RawTokenType type, std::string data) :
    type(type),
    data(std::move(data)) {
}

SPP_MOD_END
