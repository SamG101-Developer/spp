module;
#include <spp/macros.hpp>

export module spp.asts.fold_expression_ast;
import spp.asts.primary_expression_ast;
import std;


SPP_EXP_CLS struct spp::asts::FoldExpressionAst final : PrimaryExpressionAst {
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

    SPP_AST_KEY_FUNCTIONS;
};
