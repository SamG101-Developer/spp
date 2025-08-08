#include <spp/asts/token_ast.hpp>


spp::asts::TokenAst::TokenAst(
    const std::size_t pos,
    const lex::SppTokenType token_type,
    std::string &&token_data) :
    token_type(token_type),
    token_data(std::move(token_data)),
    m_pos(pos) {
}


auto spp::asts::TokenAst::operator==(TokenAst const &that) const -> bool {
    return token_type == that.token_type;
}


auto spp::asts::TokenAst::pos_start() const -> std::size_t {
    return m_pos;
}


auto spp::asts::TokenAst::pos_end() const -> std::size_t {
    return m_pos + token_data.length();
}


auto spp::asts::TokenAst::print(meta::AstPrinter &) const -> std::string {
    return token_data;
}


spp::asts::TokenAst::operator std::string() const {
    return token_data;
}
