module;
#include <spp/macros.hpp>

module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::LocalVariableSingleIdentifierAliasAst::LocalVariableSingleIdentifierAliasAst(
    decltype(TokAs) &&tok_as,
    decltype(Name) &&name) :
    TokAs(std::move(tok_as)),
    Name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAs, lex::SppTokenType::KW_AS, "as");
}

spp::asts::LocalVariableSingleIdentifierAliasAst::~LocalVariableSingleIdentifierAliasAst() = default;

auto spp::asts::LocalVariableSingleIdentifierAliasAst::PosStart() const
    -> std::size_t {
    // Use the "as" token.
    return TokAs->PosStart();
}

auto spp::asts::LocalVariableSingleIdentifierAliasAst::PosEnd() const
    -> std::size_t {
    // Use the alias name.
    return Name->PosEnd();
}

auto spp::asts::LocalVariableSingleIdentifierAliasAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LocalVariableSingleIdentifierAliasAst>(
        AstClone(TokAs), AstClone(Name));
}

auto spp::asts::LocalVariableSingleIdentifierAliasAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokAs).append_range(" ");
    SPP_STRING_APPEND(Name);
    SPP_STRING_END;
}

SPP_MOD_END
