module;
#include <spp/macros.hpp>

export module spp.asts.fold_expression_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FoldExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::FoldExpressionAst final : PrimaryExpressionAst {
    /**
     * The @c .. fold token that indicates a fold operation. Used in binary and function call contexts.
     */
    Unique<TokenAst> TokEllipsis;

    /**
     * Construct the FoldExpressionAst with the arguments matching the members.
     * @param[in] tok_ellipsis The @c .. fold token that indicates a fold operation.
     */
    explicit FoldExpressionAst(
        decltype(TokEllipsis) &&tok_ellipsis);

    ~FoldExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};
