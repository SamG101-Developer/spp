module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.expression_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.algorithms;

SPP_MOD_BEGIN
spp::asts::ClosureExpressionParameterAndCaptureGroupAst::ClosureExpressionParameterAndCaptureGroupAst(
    decltype(TokL) &&tok_l,
    decltype(ParamGroup) &&param_group,
    decltype(CaptureGroup) &&capture_group,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    ParamGroup(std::move(param_group)),
    CaptureGroup(std::move(capture_group)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->CaptureGroup)
}

spp::asts::ClosureExpressionParameterAndCaptureGroupAst::~ClosureExpressionParameterAndCaptureGroupAst() = default;

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ClosureExpressionParameterAndCaptureGroupAst>(
        AstClone(TokL), AstClone(ParamGroup), AstClone(CaptureGroup), AstClone(TokR));
}

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(ParamGroup);
    SPP_STRING_APPEND(CaptureGroup);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the arguments against the outer scope's symbols (temp move asts).
    auto caps = CaptureGroup->Captures
        | spp::views::move
        | spp::views::cast_unique<FunctionCallArgumentAst>
        | std::ranges::to<Vec>();

    const auto cap_group = MakeUnique<FunctionCallArgumentGroupAst>(
        nullptr, std::move(caps), nullptr);
    cap_group->Stage7_AnalyseSemantics(sm, meta);

    // New scope for parameters.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "closure-outer", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    CaptureGroup->Captures = cap_group->Args
        | spp::views::move
        | spp::views::cast_unique<ClosureExpressionCaptureAst>
        | std::ranges::to<Vec>();

    // Analyse the parameters and captures.
    ParamGroup->Stage7_AnalyseSemantics(sm, meta);
    CaptureGroup->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the arguments against the outer scope's symbols (temp move asts).
    meta->CurrentLambdaOuterScope = sm->CurrentScope;
    auto caps = CaptureGroup->Captures
        | spp::views::move
        | spp::views::cast_unique<FunctionCallArgumentAst>
        | std::ranges::to<Vec>();
    const auto cap_group = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, std::move(caps), nullptr);
    cap_group->Stage8_CheckMemory(sm, meta);

    // New scope for parameters.
    sm->MoveToNextScope();
    CaptureGroup->Captures = cap_group->Args
        | spp::views::move
        | spp::views::cast_unique<ClosureExpressionCaptureAst>
        | std::ranges::to<Vec>();

    // Check the parameters and captures.
    ParamGroup->Stage8_CheckMemory(sm, meta);
    CaptureGroup->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the parameters into the current scope.
    meta->CurrentLambdaOuterScope = sm->CurrentScope;
    sm->MoveToNextScope();
    ParamGroup->Stage11_CodeGen(sm, meta, ctx);
    CaptureGroup->Stage11_CodeGen(sm, meta, ctx);
    return nullptr;
}

SPP_MOD_END
