module;
#include <spp/macros.hpp>

export module spp.asts.loop_iterable_expression_ast;
import spp.asts.loop_expression_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS struct LoopConditionalExpressionAst;
    SPP_EXP_CLS struct LoopIterableExpressionAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopIterableExpressionAst final : LoopExpressionAst {
private:
    std::unique_ptr<LetStatementInitializedAst> m_transform_let;
    std::unique_ptr<LoopConditionalExpressionAst> m_transform_loop;

public:
    /**
     * The variable for iteration. This is filled with each element of the iterable as the loop iterates.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * The token that represents the @c in keyword in the loop condition. This is used to indicate that the loop is
     * iterating over an iterable, and separates the variable from the iterable.
     */
    std::unique_ptr<TokenAst> tok_in;

    /**
     * The iterable expression that the loop will iterate over. This can be any expression that evaluates to a generator
     * type, typically the @code .iter_xxx@endcode family of methods.
     */
    std::unique_ptr<ExpressionAst> iterable;

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
        decltype(tok_loop) &&tok_loop,
        decltype(var) &&var,
        decltype(tok_in) &&tok_in,
        decltype(iterable) &&iterable,
        decltype(body) &&body,
        decltype(else_block) &&else_block);

    ~LoopIterableExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
