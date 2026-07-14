module;
#include <spp/macros.hpp>

export module spp.asts.convention_mut_ast;
import spp.asts.convention_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionMutAst;
    SPP_EXP_CLS struct TokenAst;
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
    Unique<TokenAst> TokBorrow;

    /**
     * The token that represents the @c mut keyword. This is used to indicate that the borrow is mutable, and that the
     * value can be modified.
     */
    Unique<TokenAst> TokMut;

    /**
     * Construct the ConventionMutAst with the arguments matching the members.
     * @param tok_borrow The token that represents the @c & borrow marker.
     * @param tok_mut The token that represents the @c mut keyword.
     */
    ConventionMutAst(
        decltype(TokBorrow) &&tok_borrow,
        decltype(TokMut) &&tok_mut);

    ~ConventionMutAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
