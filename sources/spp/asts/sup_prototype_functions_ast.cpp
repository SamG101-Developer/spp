#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::SupPrototypeFunctionsAst::SupPrototypeFunctionsAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(body) &&body):
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    body(std::move(body)) {
}


auto spp::asts::SupPrototypeFunctionsAst::pos_start() const -> std::size_t {
    return tok_sup->pos_start();
}


auto spp::asts::SupPrototypeFunctionsAst::pos_end() const -> std::size_t {
    return body->pos_end();
}


spp::asts::SupPrototypeFunctionsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sup);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::SupPrototypeFunctionsAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sup);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}
