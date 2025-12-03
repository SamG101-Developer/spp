module;
#include <spp/macros.hpp>

export module spp.asts.loop_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct LoopConditionAst;
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopExpressionAst final : PrimaryExpressionAst {
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

    SPP_AST_KEY_FUNCTIONS;

private:
    auto m_codegen_condition_bool(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) const -> llvm::Value*;

    auto m_codegen_condition_iter(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) const -> llvm::Value*;

public:
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
