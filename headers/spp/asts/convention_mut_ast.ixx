module;
#include <spp/macros.hpp>

export module spp.asts.convention_mut_ast;
import spp.asts.convention_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionMutAst;
}


/**
 * The ConventionMutAst represents a convention for mutable borrows. If the borrow is for an argument, this symbol must
 * be mutably defined.
 */
SPP_EXP_CLS struct spp::asts::ConventionMutAst final : ConventionAst {
    /**
     * The token that represents the @c & borrow marker. This is used to indicate that a borrow of some convention is
     * being made.
     */
    std::unique_ptr<TokenAst> tok_borrow;

    /**
     * The token that represents the @c mut keyword. This is used to indicate that the borrow is mutable, and that the
     * value can be modified.
     */
    std::unique_ptr<TokenAst> tok_mut;

    /**
     * Construct the ConventionMutAst with the arguments matching the members.
     * @param tok_borrow The token that represents the @c & borrow marker.
     * @param tok_mut The token that represents the @c mut keyword.
     */
    ConventionMutAst(
        decltype(tok_borrow) &&tok_borrow,
        decltype(tok_mut) &&tok_mut);

    ~ConventionMutAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
