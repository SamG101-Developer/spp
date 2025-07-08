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
     * The @c skip token that indicates the loop should be skipped. This can be used without @c exit tokens, or with
     * @c exit tokens; @code exit exit skip@endcode will exit the innermost 2 loops, then skip the iteration of the 3rd
     * loop. If a value is present, then @code exit 5@endcode will exit the loop and pass the value into the variable
     * whose value is the loop expression.
     */
    std::unique_ptr<ExpressionAst> tok_skip_or_expr;

    /**
     * Construct the LoopControlFlowStatementAst with the arguments matching the members.
     * @param tok_seq_exit The list of @c exit tokens that indicate the loop should be exited.
     * @param tok_skip_or_expr The @c skip token that indicates the loop should be skipped, or an expression that is
     * passed to the loop variable.
     */
    LoopControlFlowStatementAst(
        decltype(tok_seq_exit) &&tok_seq_exit,
        decltype(tok_skip_or_expr) &&tok_skip_or_expr);
};


#endif //LOOP_CONTROL_FLOW_STATEMENT_AST_HPP
