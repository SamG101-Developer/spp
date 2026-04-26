module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.object_initializer_argument_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentShorthandAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The ObjectInitializerArgumentShorthandAst represents a shorthand argument in an object initializer. It is forces the
 * argument to be matched by shorthand value rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentShorthandAst final : ObjectInitializerArgumentAst {
    /**
     * The optional @c .. token that indicates an "else" argument. This fills all the missing attributes in the object
     * with the corresponding attributes from this argument.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * Factory function to create a shorthand argument, with the provided expression, that is used for "autofill", ie
     * the "..arg" argument => fill all missing field from this object into the new initializer.
     * @param val The expression to move fields off of.
     * @return The "..arg" form.
     */
    static auto create_autofill(
        std::unique_ptr<ExpressionAst> &&val)
        -> std::unique_ptr<ObjectInitializerArgumentShorthandAst>;

    /**
     * Construct the ObjectInitializerArgumentShorthandAst with the arguments matching the members.
     * @param tok_ellipsis The optional @c .. token that indicates an "else" argument.
     * @param val The expression that is being passed as the argument to the object initializer.
     *
     * @note This constructor just calls the ObjectInitializerArgumentAst constructor with the same arguments, but is
     * defined for uniformity with the other argument variants.
     */
    explicit ObjectInitializerArgumentShorthandAst(
        std::unique_ptr<TokenAst> tok_ellipsis,
        std::unique_ptr<ExpressionAst> &&val);

    ~ObjectInitializerArgumentShorthandAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
