module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_slice_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorSliceAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorSliceAst final : PostfixExpressionOperatorAst {
    /**
     * The @code [@endcode token that indicates the start of the slice expression.
     */
    Unique<TokenAst> TokL;

    /**
     * The optional @c mut token that indicates that the slice operation is mutable.
     */
    Unique<TokenAst> TokMut;

    /**
     * The lower bound expression used for the slice.
     */
    Unique<ExpressionAst> ExprLBound;

    /**
     * The @c to keyword, separating the bounds of the slice.
     */
    Unique<TokenAst> TokTo;

    /**
     * The upper bound expression used for the slice.
     */
    Unique<ExpressionAst> ExprRBound;

    /**
     * The @code ]@endcode token that indicates the end of the slice expression.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the PostfixExpressionOperatorIndexAst with the arguments matching the members.
     * @param[in] tok_l The @code [@endcode token that indicates the start of the index expression.
     * @param[in] tok_mut The optional @c mut token that indicates that the index operation is mutable.
     * @param[in] expr_l_bound The lower bound expression used for the slice.
     * @param[in] tok_to The @c to keyword, separating the bounds of the slice.
     * @param[in] expr_r_bound The upper bound expression used for the slice.
     * @param[in] tok_r The @code ]@endcode token that indicates the end of the index expression.
     */
    PostfixExpressionOperatorSliceAst(
        decltype(TokL) &&tok_l,
        decltype(TokMut) &&tok_mut,
        decltype(ExprLBound) &&expr_l_bound,
        decltype(TokTo) &&tok_to,
        decltype(ExprRBound) &&expr_r_bound,
        decltype(TokR) &&tok_r);

    /**
     * Default destructor.
     */
    ~PostfixExpressionOperatorSliceAst() override;

    SPP_AST_KEY_FUNCTIONS;

    /**
     * The analysis stage requires that the left-hand-side is "sliceable", in the same way that an iterator-based loop
     * expression's condition must be "iterable". Either @c SliceRef or @c SliceMut must be superimposed over the
     * left-hand-side, depending on the presence of the @c mut keyword.
     * @param[in,out] sm The scope manager to use for analysis.
     * @param[in,out] meta Associated metadata.
     */
    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * Type inference is done with the mapped function for the @c index operator on the left-hand-side type. For
     * example, @code "hello"[5 to 7]@endcode will become @code "hello".slice_ref(5, 6)@endcode, and this method's
     * return type will be used.
     * @param[in,out] sm The scope manager to use for type inference.
     * @param[in,out] meta Associated metadata.
     * @return
     */
    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

private:
    Unique<PostfixExpressionAst> _MappedFunc;
};
