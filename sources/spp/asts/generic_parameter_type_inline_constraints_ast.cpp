module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::GenericParameterTypeInlineConstraintsAst::GenericParameterTypeInlineConstraintsAst(
    decltype(tok_colon) &&tok_colon,
    decltype(constraints) &&constraints) :
    tok_colon(std::move(tok_colon)),
    constraints(std::move(constraints)) {
}


spp::asts::GenericParameterTypeInlineConstraintsAst::~GenericParameterTypeInlineConstraintsAst() = default;


auto spp::asts::GenericParameterTypeInlineConstraintsAst::pos_start() const
    -> std::size_t {
    return tok_colon->pos_start();
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::pos_end() const
    -> std::size_t {
    return constraints.empty() ? tok_colon->pos_end() : constraints.back()->pos_end();
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeInlineConstraintsAst>(
        ast_clone(tok_colon),
        ast_clone_vec(constraints));
}


spp::asts::GenericParameterTypeInlineConstraintsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_EXTEND(constraints, " & ");
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_EXTEND(constraints, " & ");
    SPP_PRINT_END;
}
