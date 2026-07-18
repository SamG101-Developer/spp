module;
#include <spp/macros.hpp>

export module spp.asts.loop_control_flow_statement_ast;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct LoopControlFlowStatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopControlFlowStatementAst final : StatementAst {
    /**
     * The list of @c exit tokens. This allows for a statement to exit an arbitrary number of loops. If there are no
     * @c exit tokens, then the @c skip token will be present, in @c tok_skip_or_expr. This is ensured by the parser.
     */
    Vec<Unique<TokenAst>> TokSeqExit;

    /**
     * The optional @c skip token that indicates the loop should be skipped. This can be used without @c exit tokens, or
     * with @c exit tokens; @code exit exit skip@endcode will exit the innermost 2 loops, then skip the iteration of the
     * 3rd loop. A @c skip and a value are mutually exclusive, and the parser prevents both from being present.
     */
    Unique<TokenAst> TokSkip;

    /**
     * The expression that is returned to the assignment variable of the loop. For example, with the statement
     * @code let x = loop { ... }@endcode, @c x can be assigned a value with the @code exit value@endcode statement.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Factory to create a single "skip" control flow statement. Used when generating boolean conditional loops from an
     * iterable loop.
     * @param pos Token position for the statement.
     * @return The generated "skip" statement.
     */
    static auto Skip(
        std::size_t pos)
        -> Unique<LoopControlFlowStatementAst>;

    /**
     * Factory to create a single "exit" control flow statement. Used when generating boolean conditional loops from an
     * iterable loop, and when desugaring "break" statements in boolean conditional loops. The number of @c exit tokens
     * is determined by the @c depth argument, which indicates how many loops should be exited by the statement. No
     * expression will be bound to this "exit" statement.
     * @param pos Token position for the statement.
     * @param depth The number of @c exit tokens to generate, which indicates how many loops should be exited.
     * @return The generated "exit" statement with the specified number of @c exit tokens.
     */
    static auto Exit(
        std::size_t pos,
        std::size_t depth)
        -> Unique<LoopControlFlowStatementAst>;

    /**
     * Construct the LoopControlFlowStatementAst with the arguments matching the members.
     * @param tok_seq_exit The list of @c exit tokens that indicate the loop should be exited.
     * @param tok_skip The optional @c skip token that indicates the loop should be skipped.
     * @param expr The optional expression that is returned to the assignment variable of the loop.
     * passed to the loop variable.
     */
    LoopControlFlowStatementAst(
        decltype(TokSeqExit) &&tok_seq_exit,
        decltype(TokSkip) &&tok_skip,
        decltype(Expr) &&expr);

    ~LoopControlFlowStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
