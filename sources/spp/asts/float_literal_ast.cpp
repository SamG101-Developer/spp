#include <algorithm>

#include <spp/asts/float_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FloatLiteralAst::FloatLiteralAst(
    decltype(tok_sign) &&tok_sign,
    decltype(int_val) &&int_val,
    decltype(tok_dot) &&tok_dot,
    decltype(frac_val) &&frac_val,
    icu::UnicodeString &&):
    tok_sign(std::move(tok_sign)),
    int_val(std::move(int_val)),
    tok_dot(std::move(tok_dot)),
    frac_val(std::move(frac_val)),
    type(nullptr) { // todo (type)
}


auto spp::asts::FloatLiteralAst::pos_start() const -> std::size_t {
    return tok_sign ? tok_sign->pos_start() : int_val->pos_start();
}


auto spp::asts::FloatLiteralAst::pos_end() const -> std::size_t {
    return type ? type->pos_end() : frac_val->pos_end();
}


spp::asts::FloatLiteralAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sign);
    SPP_STRING_APPEND(int_val);
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(frac_val);
    SPP_STRING_APPEND(type);
    SPP_STRING_END;
}


auto spp::asts::FloatLiteralAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sign);
    SPP_PRINT_APPEND(int_val);
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(frac_val);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_END;
}
