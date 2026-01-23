module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.token_ast;

import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericArgumentCompKeywordAst represents a keyword argument in a generic argument context. It is forces the argument
 * to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompKeywordAst final : GenericArgumentCompAst {
    /**
     * The name of the keyword argument. This is the identifier that is used to refer to the argument in the generic
     * call.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * The token that represents the assignment operator @c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * Construct the GenericArgumentCompKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator @c = in the keyword argument.
     * @param val The value of the generic comp argument.
     */
    GenericArgumentCompKeywordAst(
        decltype(name) name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);

    ~GenericArgumentCompKeywordAst() override;

    using GenericArgumentAst::equals;
    using GenericArgumentAst::equals_generic_argument_comp_keyword;
    SPP_ATTR_NODISCARD auto equals(GenericArgumentAst const &other) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &other) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    static auto from_symbol(analyse::scopes::VariableSymbol const &sym) -> std::unique_ptr<GenericArgumentCompKeywordAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};


spp::asts::GenericArgumentCompKeywordAst::~GenericArgumentCompKeywordAst() = default;
