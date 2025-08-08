#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/generic_parameter_type_optional_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterTypeOptionalAst::GenericParameterTypeOptionalAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val):
    GenericParameterTypeAst(std::move(name), std::move(constraints)),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


spp::asts::GenericParameterTypeOptionalAst::~GenericParameterTypeOptionalAst() = default;


auto spp::asts::GenericParameterTypeOptionalAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericParameterTypeOptionalAst::pos_end() const -> std::size_t {
    return default_val->pos_end();
}


spp::asts::GenericParameterTypeOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(constraints);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeOptionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(constraints);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}
