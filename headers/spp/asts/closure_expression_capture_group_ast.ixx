module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_group_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionCaptureAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureGroupAst final : virtual Ast {
    /**
     * The @c caps token that indicates the start of the closure capture group. This is used to indicate that the
     * closure has moved on from parameter definitions and is now capturing variables from the outer scope.
     */
    std::unique_ptr<TokenAst> tok_caps;

    /**
     * The captured variables from the outer scope. These are variables that are captured by the closure and can be used
     * within its body.
     */
    std::vector<std::unique_ptr<ClosureExpressionCaptureAst>> captures;

    /**
     * Construct the ClosureExpressionCaptureGroupAst with the arguments matching the members.
     * @param[in] tok_caps The @c caps token that indicates the start of the closure capture group.
     * @param[in] captures The captured variables from the outer scope.
     */
    explicit ClosureExpressionCaptureGroupAst(
        decltype(tok_caps) &&tok_caps,
        decltype(captures) &&captures);

    ~ClosureExpressionCaptureGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    static auto new_empty() -> std::unique_ptr<ClosureExpressionCaptureGroupAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
