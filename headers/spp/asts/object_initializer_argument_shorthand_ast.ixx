module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.object_initializer_argument_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentShorthandAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

/**
 * The ObjectInitializerArgumentShorthandAst represents a shorthand argument in an object initializer. It is forces the
 * argument to be matched by shorthand value rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentShorthandAst final : ObjectInitializerArgumentAst {
    /**
     * The optional @c .. token that indicates an "else" argument. This fills all the missing attributes in the object
     * with the corresponding attributes from this argument.
     */
    Unique<TokenAst> TokEllipsis;

    /**
     * Factory function to create a shorthand argument, with the provided expression, that is used for "autofill", ie
     * the "..arg" argument => fill all missing field from this object into the new initializer.
     * @param val The expression to move fields off of.
     * @return The "..arg" form.
     */
    static auto CreateAutoFillArg(
        Unique<ExpressionAst> &&val)
        -> Unique<ObjectInitializerArgumentShorthandAst>;

    /**
     * Construct the ObjectInitializerArgumentShorthandAst with the arguments matching the members.
     * @param tok_ellipsis The optional @c .. token that indicates an "else" argument.
     * @param val The expression that is being passed as the argument to the object initializer.
     *
     * @note This constructor just calls the ObjectInitializerArgumentAst constructor with the same arguments, but is
     * defined for uniformity with the other argument variants.
     */
    explicit ObjectInitializerArgumentShorthandAst(
        decltype(TokEllipsis) &&tok_ellipsis,
        decltype(Val) &&val);

    ~ObjectInitializerArgumentShorthandAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};
