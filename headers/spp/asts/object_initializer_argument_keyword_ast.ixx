module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerArgumentKeywordAst;
    SPP_EXP_CLS struct TokenAst;
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
    Unique<TokenAst> TokAssign;

    /**
     * Construct the ObjectInitializerArgumentKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator @c = in the keyword argument.
     * @param val The expression that is being passed as the argument to the object initializer.
     */
    ObjectInitializerArgumentKeywordAst(
        decltype(Name) name,
        decltype(TokAssign) &&tok_assign,
        decltype(Val) &&val);

    ~ObjectInitializerArgumentKeywordAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
