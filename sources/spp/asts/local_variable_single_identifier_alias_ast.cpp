module;
#include <spp/macros.hpp>

module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::LocalVariableSingleIdentifierAliasAst::LocalVariableSingleIdentifierAliasAst(
    decltype(tok_as) &&tok_as,
    decltype(name) &&name) :
    tok_as(std::move(tok_as)),
    name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_as, lex::SppTokenType::KW_AS, "as");
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
    SPP_STRING_APPEND(tok_as).append_range(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}
