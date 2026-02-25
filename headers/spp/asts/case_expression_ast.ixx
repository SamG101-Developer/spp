module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The CaseExpressionAst represents conditional branching, of either an if-like or switch-like nature. If the @c of
 * keyword is included in after the condition, then pattern matching is used, by combining the condition with partial
 * fragments that are the branches.
 */
SPP_EXP_CLS struct spp::asts::CaseExpressionAst final : PrimaryExpressionAst {
    std::unique_ptr<ExpressionAst> cond;
    std::unique_ptr<TokenAst> tok_of;
    std::vector<std::unique_ptr<CaseExpressionBranchAst>> branches;

    static auto new_non_pattern_match(
        decltype(cond) &&cond,
        std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> &&first,
        decltype(branches) &&branches) -> std::unique_ptr<CaseExpressionAst>;

    explicit CaseExpressionAst(
        decltype(cond) &&cond,
        decltype(tok_of) &&tok_of,
        decltype(branches) &&branches);
    ~CaseExpressionAst() override;
    auto to_rust() const -> std::string override;
};
