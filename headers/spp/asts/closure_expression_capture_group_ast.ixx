module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_group_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionCaptureAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureGroupAst final : Ast {
    /**
     * The @c caps token that indicates the start of the closure capture group. This is used to indicate that the
     * closure has moved on from parameter definitions and is now capturing variables from the outer scope.
     */
    Unique<TokenAst> TokCaps;

    /**
     * The captured variables from the outer scope. These are variables that are captured by the closure and can be used
     * within its body.
     */
    Vec<Unique<ClosureExpressionCaptureAst>> Captures;

    static auto NewEmpty() -> Unique<ClosureExpressionCaptureGroupAst>;

    /**
     * Construct the ClosureExpressionCaptureGroupAst with the arguments matching the members.
     * @param[in] tok_caps The @c caps token that indicates the start of the closure capture group.
     * @param[in] captures The captured variables from the outer scope.
     */
    explicit ClosureExpressionCaptureGroupAst(
        decltype(TokCaps) &&tok_caps,
        decltype(Captures) &&captures);

    ~ClosureExpressionCaptureGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
