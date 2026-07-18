module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionParameterAndCaptureGroupAst final : Ast {
    /**
     * The @c | token that indicates the start of the closure parameter and capture group.
     */
    Unique<TokenAst> TokL;

    /**
     * The parameters of the closure. This is a list of parameters that will be passed to the closure when it is called.
     */
    Unique<ClosureExpressionParameterGroupAst> ParamGroup;

    /**
     * The captured variables from the outer scope. These are variables that are captured by the closure and can be used
     * within its body.
     */
    Unique<ClosureExpressionCaptureGroupAst> CaptureGroup;

    /**
     * The @c | token that indicates the end of the closure parameter and capture group.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the ClosureExpressionParameterAndCaptureGroupAst with the arguments matching the members.
     * @param tok_l The @c | token that indicates the start of the closure parameter and capture group.
     * @param param_group The parameters of the closure.
     * @param capture_group The captured variables from the outer scope.
     * @param tok_r The @c | token that indicates the end of the closure parameter and capture group.
     */
    ClosureExpressionParameterAndCaptureGroupAst(
        decltype(TokL) &&tok_l,
        decltype(ParamGroup) &&param_group,
        decltype(CaptureGroup) &&capture_group,
        decltype(TokR) &&tok_r);

    ~ClosureExpressionParameterAndCaptureGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
