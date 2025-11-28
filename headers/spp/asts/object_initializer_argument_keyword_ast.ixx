module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts._fwd;
import spp.asts.object_initializer_argument_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerArgumentKeywordAst;
}


/**
 * The ObjectInitializerArgumentKeywordAst represents a keyword argument in a object initializer. It is forces the
 * argument to be matched by a keyword rather than shorthand value.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentKeywordAst final : ObjectInitializerArgumentAst {
    /**
     * The token that represents the assignment operator @c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * Construct the ObjectInitializerArgumentKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator @c = in the keyword argument.
     * @param val The expression that is being passed as the argument to the object initializer.
     */
    ObjectInitializerArgumentKeywordAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);

    ~ObjectInitializerArgumentKeywordAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
