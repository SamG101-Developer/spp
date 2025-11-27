module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;


spp::asts::GenericParameterTypeVariadicAst::GenericParameterTypeVariadicAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    decltype(name) &&name,
    decltype(constraints) &&constraints) :
    GenericParameterTypeAst(std::move(name), std::move(constraints), mixins::OrderableTag::VARIADIC_PARAM),
    tok_ellipsis(std::move(tok_ellipsis)) {
}


spp::asts::GenericParameterTypeVariadicAst::~GenericParameterTypeVariadicAst() = default;


auto spp::asts::GenericParameterTypeVariadicAst::pos_start() const
    -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::GenericParameterTypeVariadicAst::pos_end() const
    -> std::size_t {
    return tok_ellipsis->pos_end();
}


auto spp::asts::GenericParameterTypeVariadicAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeVariadicAst>(
        ast_clone(tok_ellipsis),
        ast_clone(name),
        ast_clone(constraints));
}


spp::asts::GenericParameterTypeVariadicAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(constraints);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeVariadicAst::print(meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(constraints);
    SPP_PRINT_END;
}
