#include <spp/asts/boolean_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/lex/tokens.hpp>


spp::asts::BooleanLiteralAst::BooleanLiteralAst(
    decltype(tok_bool) &&tok_bool):
    tok_bool(std::move(tok_bool)) {
}


auto spp::asts::BooleanLiteralAst::True(const std::size_t pos) -> std::unique_ptr<BooleanLiteralAst> {
    auto tok = std::make_unique<TokenAst>(pos, lex::SppTokenType::KW_TRUE, "true");
    return std::make_unique<BooleanLiteralAst>(std::move(tok));
}


auto spp::asts::BooleanLiteralAst::False(const std::size_t pos) -> std::unique_ptr<BooleanLiteralAst> {
    auto tok = std::make_unique<TokenAst>(pos, lex::SppTokenType::KW_FALSE, "false");
    return std::make_unique<BooleanLiteralAst>(std::move(tok));
}


auto spp::asts::BooleanLiteralAst::pos_start() const -> std::size_t {
    return tok_bool->pos_start();
}


auto spp::asts::BooleanLiteralAst::pos_end() const -> std::size_t {
    return tok_bool->pos_end();
}


spp::asts::BooleanLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_bool);
    SPP_STRING_END;
}


auto spp::asts::BooleanLiteralAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_bool);
    SPP_PRINT_END;
}
