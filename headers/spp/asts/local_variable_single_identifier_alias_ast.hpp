#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::LocalVariableSingleIdentifierAliasAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

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
};
