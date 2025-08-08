#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::IntegerLiteralAst::IntegerLiteralAst(
    decltype(sign) &&sign,
    decltype(tok_integer) &&tok_integer,
    std::string &&type):
    LiteralAst(),
    sign(std::move(sign)),
    tok_integer(std::move(tok_integer)),
    raw_type(std::move(type)) {
}


spp::asts::IntegerLiteralAst::~IntegerLiteralAst() = default;


auto spp::asts::IntegerLiteralAst::pos_start() const -> std::size_t {
    return sign ? sign->pos_start() : tok_integer->pos_start();
}


auto spp::asts::IntegerLiteralAst::pos_end() const -> std::size_t {
    return tok_integer->pos_end();
}


spp::asts::IntegerLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(sign);
    SPP_STRING_APPEND(tok_integer);
    raw_string.append(raw_type);
    SPP_STRING_END;
}


auto spp::asts::IntegerLiteralAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(sign);
    SPP_PRINT_APPEND(tok_integer);
    formatted_string.append(raw_type);
    SPP_PRINT_END;
}
