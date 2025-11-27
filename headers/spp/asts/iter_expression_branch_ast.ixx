module;
#include <spp/macros.hpp>

export module spp.asts.iter_expression_branch_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;

import std;


SPP_EXP_CLS struct spp::asts::IterExpressionBranchAst final : virtual Ast, mixins::TypeInferrableAst {
    /**
     * The pattern that this branch matches against. This can only be a single pattern due to different @c iter patterns
     * introducing different variables and symbols (same as case-of destructuring).
     */
    std::unique_ptr<IterPatternVariantAst> pattern;

    /**
     * The body of the iteration branch. This is an inner scope that contains the statements that will be executed if
     * the branch matches.
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * An optional guard for the iteration branch. This is a boolean expression that must evaluate to true for the
     * branch to be executed.
     */
    std::unique_ptr<PatternGuardAst> guard;

    /**
     * Construct the IterExpressionBranchAst with the arguments matching the members.
     * @param pattern The pattern that this branch matches against.
     * @param body The body of the iteration branch.
     * @param guard The optional guard for the iteration branch.
     */
    IterExpressionBranchAst(
        decltype(pattern) &&pattern,
        decltype(body) &&body,
        decltype(guard) &&guard);

    ~IterExpressionBranchAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
