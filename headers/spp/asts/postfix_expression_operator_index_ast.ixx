module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_index_ast;
import spp.asts.postfix_expression_operator_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorIndexAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorIndexAst final : PostfixExpressionOperatorAst {
private:
    std::shared_ptr<PostfixExpressionAst> m_mapped_func;

public:
    /**
     * The @code [@endcode token that indicates the start of the index expression.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The optional @c mut token that indicates that the index operation is mutable.
     */
    std::unique_ptr<TokenAst> tok_mut;

    /**
     * The expression used to index into the lhs expression.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * The @code ]@endcode token that indicates the end of the index expression.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the PostfixExpressionOperatorIndexAst with the arguments matching the members.
     * @param[in] tok_l The @code [@endcode token that indicates the start of the index expression.
     * @param[in] tok_mut The optional @c mut token that indicates that the index operation is mutable.
     * @param[in] expr The expression used to index into the lhs expression.
     * @param[in] tok_r The @code ]@endcode token that indicates the end of the index expression.
     */
    PostfixExpressionOperatorIndexAst(
        std::unique_ptr<TokenAst> tok_l,
        std::unique_ptr<TokenAst> tok_mut,
        std::unique_ptr<ExpressionAst> expr,
        std::unique_ptr<TokenAst> tok_r);

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
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Type inference is done with the mapped function for the @c index operator on the left-hand-side type. For
     * example, @code "hello"[5]@endcode will become @code "hello".index_ref(5)@endcode, and this method's return type
     * will be used.
     * @param[in,out] sm The scope manager to use for type inference.
     * @param[in,out] meta Associated metadata.
     * @return
     */
    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
