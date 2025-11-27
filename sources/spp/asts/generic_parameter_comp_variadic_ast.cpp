module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;


spp::asts::GenericParameterCompVariadicAst::GenericParameterCompVariadicAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(tok_ellipsis) &&tok_ellipsis,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type), mixins::OrderableTag::VARIADIC_PARAM),
    tok_ellipsis(std::move(tok_ellipsis)) {
}


spp::asts::GenericParameterCompVariadicAst::~GenericParameterCompVariadicAst() = default;


auto spp::asts::GenericParameterCompVariadicAst::pos_start() const
    -> std::size_t {
    return tok_cmp->pos_start();
}


auto spp::asts::GenericParameterCompVariadicAst::pos_end() const
    -> std::size_t {
    return tok_ellipsis->pos_end();
}


auto spp::asts::GenericParameterCompVariadicAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterCompVariadicAst>(
        ast_clone(tok_cmp),
        ast_clone(tok_ellipsis),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type));
}


spp::asts::GenericParameterCompVariadicAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_cmp).append(" ");
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterCompVariadicAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_cmp).append(" ");
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon).append(" ");
    SPP_PRINT_APPEND(type);
    SPP_PRINT_END;
}
