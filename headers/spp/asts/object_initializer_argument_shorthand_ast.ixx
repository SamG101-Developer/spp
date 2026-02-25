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
    std::unique_ptr<TokenAst> tok_ellipsis;

    explicit ObjectInitializerArgumentShorthandAst(
        std::unique_ptr<TokenAst> tok_ellipsis,
        std::unique_ptr<ExpressionAst> &&val);
    ~ObjectInitializerArgumentShorthandAst() override;
    auto to_rust() const -> std::string override;
};
