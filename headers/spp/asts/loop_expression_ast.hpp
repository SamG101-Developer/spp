#pragma once
#include <spp/asts/primary_expression_ast.hpp>


struct spp::asts::LoopExpressionAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;
private:
    std::optional<std::tuple<ExpressionAst*, std::shared_ptr<TypeAst>, analyse::scopes::Scope*>> m_loop_exit_type_info;

public:
    /**
     * The @c loop token that indicates the start of a loop expression.
     */
    std::unique_ptr<TokenAst> tok_loop;

    /**
     * The condition of the loop. This will be either boolean, or iterable if the "loop-in" syntax is used.
     */
    std::unique_ptr<LoopConditionAst> cond;

    /**
     * The body of the loop. This is a block of statements that will be executed for each iteration of the loop.
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * The optional @c else block of the loop. This will be executed if the original condition is immediately false, or
     * the iterable is already exhausted (no loops take place).
     */
    std::unique_ptr<LoopElseStatementAst> else_block;

    /**
     * Construct the LoopExpressionAst with the arguments matching the members.
     * @param[in] tok_loop The @c loop token that indicates the start of a loop expression.
     * @param[in] cond The condition of the loop.
     * @param[in] body The body of the loop.
     * @param[in] else_block The optional @c else block of the loop.
     */
    LoopExpressionAst(
        decltype(tok_loop) &&tok_loop,
        decltype(cond) &&cond,
        decltype(body) &&body,
        decltype(else_block) &&else_block);

    ~LoopExpressionAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
