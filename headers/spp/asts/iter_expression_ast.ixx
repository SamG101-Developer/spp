module;
#include <spp/macros.hpp>

export module spp.asts.iter_expression_ast;
import spp.asts.primary_expression_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct IterExpressionAst;
    SPP_EXP_CLS struct IterExpressionBranchAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IterExpressionAst final : PrimaryExpressionAst {
    /**
     * The @c iter token that indicates the start of an iteration expression.
     */
    std::unique_ptr<TokenAst> tok_iter;

    /**
     * The generated value being inspected. This will be from resuming a coroutine.
     */
    std::unique_ptr<ExpressionAst> cond;

    /**
     * The @c of keyword that indicated pattern matching. This mirrors the @c case-of syntax, but is used for a
     * coroutine generated value.
     */
    std::unique_ptr<TokenAst> tok_of;

    /**
     * The body of the iteration expression. This is a inner scope of @c iter block branches that each contain a
     * pattern, like @c case branches do.
     */
    std::vector<std::unique_ptr<IterExpressionBranchAst>> branches;

    /**
     * Construct the IterExpressionAst with the arguments matching the members.
     * @param[in] tok_iter The @c iter token that indicates the start of an iteration expression.
     * @param[in] cond The generated value being inspected.
     * @param[in] tok_of The @c of keyword that indicated pattern matching.
     * @param[in] branches The body of the iteration expression.
     */
    IterExpressionAst(
        decltype(tok_iter) &&tok_iter,
        decltype(cond) &&cond,
        decltype(tok_of) &&tok_of,
        decltype(branches) &&branches);

    ~IterExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
