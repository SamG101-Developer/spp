#pragma once
#include <spp/asts/generic_argument_comp_ast.hpp>


namespace spp::analyse::scopes {
    struct VariableSymbol;
}

/**
 * The GenericArgumentCompKeywordAst represents a keyword argument in a generic argument context. It is forces the argument
 * to be matched by a keyword rather than an index.
 */
struct spp::asts::GenericArgumentCompKeywordAst final : GenericArgumentCompAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name of the keyword argument. This is the identifier that is used to refer to the argument in the generic
     * call.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * The token that represents the assignment operator \c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

protected:
    auto equals(GenericArgumentAst const &other) const -> bool override;

    auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &) const -> bool override;

public:
    /**
     * Construct the GenericArgumentCompKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator \c = in the keyword argument.
     * @param val The value of the generic comp argument.
     */
    GenericArgumentCompKeywordAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);

    ~GenericArgumentCompKeywordAst() override;

    static auto from_symbol(
        analyse::scopes::VariableSymbol const *sym)
        -> std::unique_ptr<GenericArgumentCompKeywordAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
