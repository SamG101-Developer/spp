module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_comp_required_ast;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;


spp::asts::GenericParameterCompRequiredAst::GenericParameterCompRequiredAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type), mixins::OrderableTag::REQUIRED_PARAM) {
}


spp::asts::GenericParameterCompRequiredAst::~GenericParameterCompRequiredAst() = default;


auto spp::asts::GenericParameterCompRequiredAst::pos_start() const
    -> std::size_t {
    return tok_cmp->pos_start();
}


auto spp::asts::GenericParameterCompRequiredAst::pos_end() const
    -> std::size_t {
    return type->pos_end();
}


auto spp::asts::GenericParameterCompRequiredAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterCompRequiredAst>(
        ast_clone(tok_cmp),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type));
}


spp::asts::GenericParameterCompRequiredAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_cmp).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterCompRequiredAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_cmp).append(" ");
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon).append(" ");
    SPP_PRINT_APPEND(type);
    SPP_PRINT_END;
}
