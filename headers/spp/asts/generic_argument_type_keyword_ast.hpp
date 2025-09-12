#pragma once
#include <spp/asts/generic_argument_type_ast.hpp>


/**
 * The GenericArgumentTypeKeywordAst represents a keyword argument in a generic argument context. It is forces the
 * argument to be matched by a keyword rather than an index.
 */
struct spp::asts::GenericArgumentTypeKeywordAst final : GenericArgumentTypeAst {
    SPP_AST_KEY_FUNCTIONS;

protected:
    auto equals(GenericArgumentAst const &other) const -> std::strong_ordering override;

    auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &) const -> std::strong_ordering override;

public:
    /**
     * The name of the keyword argument. This is the type that is used to refer to the argument in the generic call.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * The token that represents the assignment operator \c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * Construct the GenericArgumentTypeKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator \c = in the keyword argument.
     * @param val The value of the generic type argument.
     */
    GenericArgumentTypeKeywordAst(
        decltype(name) name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) val);

    ~GenericArgumentTypeKeywordAst() override;

    static auto from_symbol(analyse::scopes::TypeSymbol const &sym) -> std::unique_ptr<GenericArgumentTypeKeywordAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
