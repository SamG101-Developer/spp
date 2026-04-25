module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


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

    /**
     * Analyse the components of the "case" block, including the branches (nested analysis). Also checks that the "else"
     * is the final branch, and that "is" destructures only match 1 pattern.
     * @param sm The scope manager to use for analysis.
     * @param meta Associated metadata.
     */
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Validate the condition's memory status, and then validate the consistency of the memory state within the
     * branches. This only triggers if an inconsistently initialized/pinned symbol is used later in the function.
     * @param sm The scope manager to use for memory checking.
     * @param meta Associated metadata.
     */
    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Compute the branches at compile-time, by evaluating the condition and then evaluating the branches in order until
     * a match is found. Will inspect the matched branch for the final comptime value.
     * @param sm
     * @param meta
     */
    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    /**
     * A @c case block only terminates (is terminatable) if one or more of its branches can terminate. This is because
     * it has to be assumed that the terminating branch will execute, in order to cover all bases.
     * @return If one of the branches can terminate.
     */
    SPP_ATTR_NODISCARD auto terminates() const -> bool override;
};
