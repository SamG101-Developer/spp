module;
#include <spp/macros.hpp>

export module spp.asts.loop_iterable_expression_ast;
import spp.asts.loop_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LoopConditionalExpressionAst;
    SPP_EXP_CLS struct LoopIterableExpressionAst;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::LoopIterableExpressionAst final : LoopExpressionAst {
    /**
     * The variable for iteration. This is filled with each element of the iterable as the loop iterates.
     */
    Unique<LocalVariableAst> Var;

    /**
     * The token that represents the @c in keyword in the loop condition. This is used to indicate that the loop is
     * iterating over an iterable, and separates the variable from the iterable.
     */
    Unique<TokenAst> TokIn;

    /**
     * The iterable expression that the loop will iterate over. This can be any expression that evaluates to a generator
     * type, typically the @code .iter_xxx@endcode family of methods.
     */
    Unique<ExpressionAst> Iterable;

    /**
     * Construct the LoopExpressionAst with the arguments matching the members.
     * @param[in] tok_loop The @c loop token that indicates the start of a loop expression.
     * @param[in] var The variable for iteration.
     * @param[in] tok_in The token that represents the @c in keyword in the loop condition.
     * @param[in] iterable The iterable expression that the loop will iterate over.
     * @param[in] body The body of the loop.
     * @param[in] else_block The optional @c else block of the loop.
     */
    LoopIterableExpressionAst(
        decltype(TokLoop) &&tok_loop,
        decltype(Var) &&var,
        decltype(TokIn) &&tok_in,
        decltype(Iterable) &&iterable,
        decltype(Body) &&body,
        decltype(ElseBlock) &&else_block);

    ~LoopIterableExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

private:
    Unique<LetStatementInitializedAst> _TransformedLet;
    Unique<LoopConditionalExpressionAst> _TransformedLoop;
};
