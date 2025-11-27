module;
#include <spp/macros.hpp>

export module spp.asts.convention_ref_ast;
import spp.asts.convention_ast;

import std;


/**
 * The ConventionRefAst represents a convention for immutable borrows. Immutable borrows can be taken from immutable or
 * mutable values.
 */
SPP_EXP_CLS struct spp::asts::ConventionRefAst final : ConventionAst {
    /**
     * The token that represents the @c & borrow marker. This is used to indicate that a borrow of some convention is
     * being made.
     */
    std::unique_ptr<TokenAst> tok_borrow;

    /**
     * Construct the ConventionRefAst with the arguments matching the members.
     * @param tok_borrow The token that represents the @c & borrow marker.
     */
    explicit ConventionRefAst(
        decltype(tok_borrow) &&tok_borrow);

    ~ConventionRefAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
