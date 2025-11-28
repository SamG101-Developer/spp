module;
#include <spp/macros.hpp>

module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;


spp::asts::LocalVariableSingleIdentifierAliasAst::LocalVariableSingleIdentifierAliasAst(
    decltype(tok_as) &&tok_as,
    decltype(name) &&name) :
    tok_as(std::move(tok_as)),
    name(std::move(name)) {
}


spp::asts::LocalVariableSingleIdentifierAliasAst::~LocalVariableSingleIdentifierAliasAst() = default;


auto spp::asts::LocalVariableSingleIdentifierAliasAst::pos_start() const
    -> std::size_t {
    return tok_as->pos_start();
}


auto spp::asts::LocalVariableSingleIdentifierAliasAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::LocalVariableSingleIdentifierAliasAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableSingleIdentifierAliasAst>(
        ast_clone(tok_as),
        ast_clone(name));
}


spp::asts::LocalVariableSingleIdentifierAliasAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_as);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableSingleIdentifierAliasAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_as);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}
