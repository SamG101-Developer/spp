module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct TypeSymbol;
}


/**
 * The GenericArgumentTypeKeywordAst represents a keyword argument in a generic argument context. It is forces the
 * argument to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeKeywordAst final : GenericArgumentTypeAst {
    /**
     * The name of the keyword argument. This is the type that is used to refer to the argument in the generic call.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * The token that represents the assignment operator @c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * Construct the GenericArgumentTypeKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator @c = in the keyword argument.
     * @param val The value of the generic type argument.
     */
    GenericArgumentTypeKeywordAst(
        decltype(name) name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) val);

    ~GenericArgumentTypeKeywordAst() override;

    SPP_ATTR_NODISCARD auto equals(GenericArgumentAst const &other) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &other) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    static auto from_symbol(analyse::scopes::TypeSymbol const &sym) -> std::unique_ptr<GenericArgumentTypeKeywordAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
