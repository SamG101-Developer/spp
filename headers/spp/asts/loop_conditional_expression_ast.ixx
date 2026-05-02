module;
#include <spp/macros.hpp>

export module spp.asts.loop_conditional_expression_ast;
import spp.asts.loop_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LoopConditionalExpressionAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopConditionalExpressionAst final : LoopExpressionAst {
    /**
     * The condition of the loop. This will be an expression that evaluates to a boolean.
     */
    Unique<ExpressionAst> Cond;

    /**
     * Construct the LoopExpressionAst with the arguments matching the members.
     * @param[in] tok_loop The @c loop token that indicates the start of a loop expression.
     * @param[in] cond The condition of the loop.
     * @param[in] body The body of the loop.
     * @param[in] else_block The optional @c else block of the loop.
     */
    LoopConditionalExpressionAst(
        decltype(TokLoop) &&tok_loop,
        decltype(Cond) &&cond,
        decltype(Body) &&body,
        decltype(ElseBlock) &&else_block);

    ~LoopConditionalExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto Terminates() const -> bool override;
};
