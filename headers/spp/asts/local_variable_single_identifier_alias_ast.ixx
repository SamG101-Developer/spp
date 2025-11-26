module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.ast;

import std;


SPP_EXP struct spp::asts::LocalVariableSingleIdentifierAliasAst final : virtual Ast {
    /**
     * The @c as token that indicates the alias for the local variable. This separates the identifier from the alias.
     */
    std::unique_ptr<TokenAst> tok_as;

    /**
     * The identifier that is used as the alias for the local variable. This will be the name on the symbol that is
     * introduced.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * Construct the LocalVariableSingleIdentifierAliasAst with the arguments matching the members.
     * @param tok_as The @c as token that indicates the alias for the local variable.
     * @param name The identifier that is used as the alias for the local variable.
     */
    LocalVariableSingleIdentifierAliasAst(
        decltype(tok_as) &&tok_as,
        decltype(name) &&name);

    ~LocalVariableSingleIdentifierAliasAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
