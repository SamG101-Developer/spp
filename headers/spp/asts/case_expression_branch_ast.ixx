module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_branch_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CaseExpressionBranchAst final : Ast {
    std::unique_ptr<TokenAst> op;
    std::vector<std::unique_ptr<CasePatternVariantAst>> patterns;
    std::unique_ptr<PatternGuardAst> guard;
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    explicit CaseExpressionBranchAst(
        decltype(op) &&op,
        decltype(patterns) &&patterns,
        decltype(guard) &&guard,
        decltype(body) &&body);
    ~CaseExpressionBranchAst() override;
    auto to_rust() const -> std::string override;
};
