#ifndef FOLD_EXPRESSION_AST_HPP
#define FOLD_EXPRESSION_AST_HPP

#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::FoldExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c .. fold token that indicates a fold operation. Used in binary and function call contexts.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * Construct the FoldExpressionAst with the arguments matching the members.
     * @param[in] tok_ellipsis The @c .. fold token that indicates a fold operation.
     */
    explicit FoldExpressionAst(
        decltype(tok_ellipsis) &&tok_ellipsis);

    ~FoldExpressionAst() override;
};


#endif //FOLD_EXPRESSION_AST_HPP
