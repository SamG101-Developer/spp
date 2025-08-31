#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/generic_parameter_type_required_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterTypeRequiredAst::GenericParameterTypeRequiredAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints) :
    GenericParameterTypeAst(std::move(name), std::move(constraints)) {
}


spp::asts::GenericParameterTypeRequiredAst::~GenericParameterTypeRequiredAst() = default;


auto spp::asts::GenericParameterTypeRequiredAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericParameterTypeRequiredAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


auto spp::asts::GenericParameterTypeRequiredAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeRequiredAst>(
        ast_clone(name),
        ast_clone(constraints));
}


spp::asts::GenericParameterTypeRequiredAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(constraints);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeRequiredAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(constraints);
    SPP_PRINT_END;
}
