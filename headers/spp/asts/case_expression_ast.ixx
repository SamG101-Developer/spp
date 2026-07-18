module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The CaseExpressionAst represents conditional branching, of either an if-like or switch-like nature. If the @c of
 * keyword is included in after the condition, then pattern matching is used, by combining the condition with partial
 * fragments that are the branches.
 */
SPP_EXP_CLS struct spp::asts::CaseExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the @c case keyword in the case expression.
     */
    Unique<TokenAst> TokCase;

    /**
     * The condition of the case expression, which is the expression that is being matched against the cases. For
     * pattern matching, the condition may not evaluate to boolean (but combining it with the partial fragments will
     * evaluate to boolean). Otherwise, the expression itself must be boolean.
     */
    Unique<ExpressionAst> Cond;

    /**
     * The optional @c of keyword, that indicates the case expression is doing pattern matching against partial
     * fragments.
     */
    Unique<TokenAst> TokOf;

    /**
     * The inner scope of the case branches. This is where the branches of the case expression are defined, and allows
     * symbols to be created inside the @c case expression scope, but available to all branches, if need be.
     */
    UniqueVec<CaseExpressionBranchAst> Branches;

    /**
     * Construct the CaseExpressionAst with the arguments matching the members.
     * @param[in] tok_case The token that represents the @c case keyword in the case expression.
     * @param[in] cond The condition of the case expression.
     * @param[in] tok_of The optional @c of keyword.
     * @param[in] branches The inner scope of the case branches.
     */
    CaseExpressionAst(
        decltype(TokCase) &&tok_case,
        decltype(Cond) &&cond,
        decltype(TokOf) &&tok_of,
        decltype(Branches) &&branches);

    ~CaseExpressionAst() override;

    static auto NewNonPatternMatch(
        decltype(TokCase) &&tok_case,
        decltype(Cond) &&cond,
        Unique<InnerScopeExpressionAst> &&first,
        decltype(Branches) &&branches) -> Unique<CaseExpressionAst>;

    /**
     * Analyse the components of the "case" block, including the branches (nested analysis). Also checks that the "else"
     * is the final branch, and that "is" destructures only match 1 pattern.
     * @param sm The scope manager to use for analysis.
     * @param meta Associated metadata.
     */
    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Validate the condition's memory status, and then validate the consistency of the memory state within the
     * branches. This only triggers if an inconsistently initialized/pinned symbol is used later in the function.
     * @param sm The scope manager to use for memory checking.
     * @param meta Associated metadata.
     */
    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Compute the branches at compile-time, by evaluating the condition and then evaluating the branches in order until
     * a match is found. Will inspect the matched branch for the final comptime value.
     * @param sm
     * @param meta
     */
    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    /**
     * A @c case block only terminates (is terminatable) if one or more of its branches can terminate. This is because
     * it has to be assumed that the terminating branch will execute, in order to cover all bases.
     * @return If one of the branches can terminate.
     */
    SPP_ATTR_NODISCARD auto Terminates() const -> bool override;
};
