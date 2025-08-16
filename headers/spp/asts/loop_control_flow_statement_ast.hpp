#ifndef LOOP_CONTROL_FLOW_STATEMENT_AST_HPP
#define LOOP_CONTROL_FLOW_STATEMENT_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LoopControlFlowStatementAst final : StatementAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of @c exit tokens. This allows for a statement to exit an arbitrary number of loops. If there are no
     * @c exit tokens, then the @c skip token will be present, in @c tok_skip_or_expr. This is ensured by the parser.
     */
    std::vector<std::unique_ptr<TokenAst>> tok_seq_exit;

    /**
     * The optional @c skip token that indicates the loop should be skipped. This can be used without @c exit tokens, or
     * with @c exit tokens; @code exit exit skip@endcode will exit the innermost 2 loops, then skip the iteration of the
     * 3rd loop. A @c skip and a value are mutually exclusive, and the parser prevents both from being present.
     */
    std::unique_ptr<TokenAst> tok_skip;

    /**
     * The expression that is returned to the assignment variable of the loop. For example, with the statement
     * @code let x = loop { ... }@endcode, @c x can be assigned a value with the @code exit value@endcode statement.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the LoopControlFlowStatementAst with the arguments matching the members.
     * @param tok_seq_exit The list of @c exit tokens that indicate the loop should be exited.
     * @param tok_skip The optional @c skip token that indicates the loop should be skipped.
     * @param expr The optional expression that is returned to the assignment variable of the loop.
     * passed to the loop variable.
     */
    LoopControlFlowStatementAst(
        decltype(tok_seq_exit) &&tok_seq_exit,
        decltype(tok_skip) &&tok_skip,
        decltype(expr) &&expr);

    ~LoopControlFlowStatementAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //LOOP_CONTROL_FLOW_STATEMENT_AST_HPP
