#ifndef CONVENTION_REF_AST_HPP
#define CONVENTION_REF_AST_HPP

#include <spp/asts/convention_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ConventionRefAst represents a convention for immutable borrows. Immutable borrows can be taken from immutable or
 * mutable values.
 */
struct spp::asts::ConventionRefAst final : ConventionAst {
    SPP_AST_KEY_FUNCTIONS;

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
};


#endif //CONVENTION_REF_AST_HPP
