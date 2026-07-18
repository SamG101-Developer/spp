module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAliasAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableSingleIdentifierAliasAst final : Ast {
    /**
     * The @c as token that indicates the alias for the local variable. This separates the identifier from the alias.
     */
    Unique<TokenAst> TokAs;

    /**
     * The identifier that is used as the alias for the local variable. This will be the name on the symbol that is
     * introduced.
     */
    Shared<IdentifierAst> Name;

    /**
     * Construct the LocalVariableSingleIdentifierAliasAst with the arguments matching the members.
     * @param tok_as The @c as token that indicates the alias for the local variable.
     * @param name The identifier that is used as the alias for the local variable.
     */
    LocalVariableSingleIdentifierAliasAst(
        decltype(TokAs) &&tok_as,
        decltype(Name) &&name);

    ~LocalVariableSingleIdentifierAliasAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
