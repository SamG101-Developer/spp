#include <spp/asts/string_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::StringLiteralAst::StringLiteralAst(
    decltype(val) &&val):
    LiteralAst(),
    val(std::move(val)) {
}


spp::asts::StringLiteralAst::~StringLiteralAst() = default;


auto spp::asts::StringLiteralAst::pos_start() const -> std::size_t {
    return val->pos_start();
}


auto spp::asts::StringLiteralAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::StringLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::StringLiteralAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
