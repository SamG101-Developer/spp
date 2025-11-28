module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_ast;
import spp.asts._fwd;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;


/**
 * The CaseExpressionAst represents conditional branching, of either an if-like or switch-like nature. If the @c of
 * keyword is included in after the condition, then pattern matching is used, by combining the condition with partial
 * fragments that are the branches.
 */
SPP_EXP_CLS struct spp::asts::CaseExpressionAst final : PrimaryExpressionAst {
    /**
     * The token that represents the @c case keyword in the case expression.
     */
    std::unique_ptr<TokenAst> tok_case;

    /**
     * The condition of the case expression, which is the expression that is being matched against the cases. For
     * pattern matching, the condition may not evaluate to boolean (but combining it with the partial fragments will
     * evaluate to boolean). Otherwise, the expression itself must be boolean.
     */
    std::unique_ptr<ExpressionAst> cond;

    /**
     * The optional @c of keyword, that indicates the case expression is doing pattern matching against partial
     * fragments.
     */
    std::unique_ptr<TokenAst> tok_of;

    /**
     * The inner scope of the case branches. This is where the branches of the case expression are defined, and allows
     * symbols to be created inside the @c case expression scope, but available to all branches, if need be.
     */
    std::vector<std::unique_ptr<CaseExpressionBranchAst>> branches;

    /**
     * Construct the CaseExpressionAst with the arguments matching the members.
     * @param[in] tok_case The token that represents the @c case keyword in the case expression.
     * @param[in] cond The condition of the case expression.
     * @param[in] tok_of The optional @c of keyword.
     * @param[in] branches The inner scope of the case branches.
     */
    CaseExpressionAst(
        decltype(tok_case) &&tok_case,
        decltype(cond) &&cond,
        decltype(tok_of) &&tok_of,
        decltype(branches) &&branches);

    ~CaseExpressionAst() override;

    static auto new_non_pattern_match(
        decltype(tok_case) &&tok_case,
        decltype(cond) &&cond,
        std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> &&first,
        decltype(branches) &&branches) -> std::unique_ptr<CaseExpressionAst>;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value * override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
