module spp.lex.tokens;


spp::lex::RawToken::RawToken(const RawTokenType type, std::string data) :
    type(type),
    data(std::move(data)) {
}
