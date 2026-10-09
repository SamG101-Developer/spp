module;
#include <spp/macros.hpp>

module spp.lex.tokens;

SPP_MOD_BEGIN
spp::lex::RawToken::RawToken(const RawTokenType type, Str data) :
  Type(type),
  Data(std::move(data)) {
}

SPP_MOD_END
