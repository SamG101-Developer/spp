module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_index_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorIndexAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorIndexAst final : PostfixExpressionOperatorAst {
    /**
     * The @code [@endcode token that indicates the start of the index expression.
     */
    Unique<TokenAst> TokL;

    /**
     * The optional @c mut token that indicates that the index operation is mutable.
     */
    Unique<TokenAst> TokMut;

    /**
     * The expression used to index into the lhs expression.
     */
    Unique<ExpressionAst> Expr;

    /**
     * The @code ]@endcode token that indicates the end of the index expression.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the PostfixExpressionOperatorIndexAst with the arguments matching the members.
     * @param[in] tok_l The @code [@endcode token that indicates the start of the index expression.
     * @param[in] tok_mut The optional @c mut token that indicates that the index operation is mutable.
     * @param[in] expr The expression used to index into the lhs expression.
     * @param[in] tok_r The @code ]@endcode token that indicates the end of the index expression.
     */
    PostfixExpressionOperatorIndexAst(
        decltype(TokL) &&tok_l,
        decltype(TokMut) &&tok_mut,
        decltype(Expr) &&expr,
        decltype(TokR) &&tok_r);

    /**
     * Default destructor.
     */
    ~PostfixExpressionOperatorIndexAst() override;

    SPP_AST_KEY_FUNCTIONS;

    /**
     * The analysis stage requires that the left-hand-side is "indexable", in the same way that an iterator-based loop
     * expression's condition must be "iterable". Either @c IterRef or @c IterMut must be superimposed over the
     * left-hand-side, depending on the presence of the @c mut keyword.
     * @param[in,out] sm The scope manager to use for analysis.
     * @param[in,out] meta Associated metadata.
     */
    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * Type inference is done with the mapped function for the @c index operator on the left-hand-side type. For
     * example, @code "hello"[5]@endcode will become @code "hello".index_ref(5)@endcode, and this method's return type
     * will be used.
     * @param[in,out] sm The scope manager to use for type inference.
     * @param[in,out] meta Associated metadata.
     * @return
     */
    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

private:
    Unique<PostfixExpressionAst> _MappedFunc;
};
