#include <spp/asts/generic_parameter_comp_optional_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterCompOptionalAst::GenericParameterCompOptionalAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type)),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


spp::asts::GenericParameterCompOptionalAst::~GenericParameterCompOptionalAst() = default;


auto spp::asts::GenericParameterCompOptionalAst::pos_start() const -> std::size_t {
    return tok_cmp->pos_start();
}


auto spp::asts::GenericParameterCompOptionalAst::pos_end() const -> std::size_t {
    return default_val->pos_end();
}


spp::asts::GenericParameterCompOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_cmp);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterCompOptionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_cmp);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}
