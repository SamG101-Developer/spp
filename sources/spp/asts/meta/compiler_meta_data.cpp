module;
#include <spp/macros.hpp>

module spp.asts.meta.compiler_meta_data;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import ankerl.unordered_dense;


SPP_MOD_BEGIN
spp::asts::meta::CompilerMetaData::CompilerMetaData() {
    CurrentStage = 0;
    ReturnTypeOverloadResolverType = nullptr;
    AssignmentTarget = nullptr;
    AssignmentTargetType = nullptr;
    IgnoreMissingElseBranchForInference = false;
    CaseCondition = nullptr;
    ClsSym = nullptr;
    EnclosingFunctionScope = nullptr;
    EnclosingFunctionFlavour = nullptr;
    EnclosingFunctionRetType = {};
    EnclosingFunctionCmp = nullptr;
    OverriddenScopeForClosure = nullptr;
    CurrentLambdaOuterScope = nullptr;
    TargetCallFunctionPrototype = nullptr;
    TargetCallWasFunctionAsync = false;
    PreventAutoGeneratorResume = false;
    LetStatementExplicitType = nullptr;
    LetStatementValue = nullptr;
    LetStatementFromUninitialized = false;
    LoopDoubleCheckActive = false;
    LoopCurrentDepth = 0;
    LoopCurrentAst = nullptr;
    LoopReturnTypes = MakeShared<ankerl::unordered_dense::map<std::size_t, std::tuple<ExpressionAst*, Shared<TypeAst>, analyse::scopes::Scope*>>>();
    ObjectInitType = nullptr;
    InferSource = {};
    InferTarget = {};
    PostfixExpressionLhs = nullptr;
    UnaryExpressionRhs = nullptr;
    SkipTypeAnalysisGenericChecks = false;
    TypeAnalysisTypeScope = nullptr;
    IgnoreCmpGeneric = nullptr;
    AllowMoveDeref = false;
    LlvmEndBB = nullptr;
    LlvmCtx = nullptr;
    LlvmAssignmentTarget = nullptr;
    LlvmAssignmentTargetType = nullptr;
    LlvmPhi = nullptr;
    CmpResult = nullptr;
}


auto spp::asts::meta::CompilerMetaData::Save() -> void {
    _History.emplace(
        CurrentStage, ReturnTypeOverloadResolverType, AssignmentTarget,
        AssignmentTargetType, IgnoreMissingElseBranchForInference, CaseCondition, ClsSym,
        OverriddenScopeForClosure, EnclosingFunctionScope, EnclosingFunctionFlavour, EnclosingFunctionRetType,
        EnclosingFunctionCmp, CurrentLambdaOuterScope, TargetCallFunctionPrototype, TargetCallWasFunctionAsync,
        PreventAutoGeneratorResume, LetStatementExplicitType, LetStatementValue, LetStatementFromUninitialized,
        LoopDoubleCheckActive, LoopCurrentDepth, LoopCurrentAst, LoopReturnTypes, ObjectInitType,
        InferSource, InferTarget, PostfixExpressionLhs, UnaryExpressionRhs, SkipTypeAnalysisGenericChecks,
        TypeAnalysisTypeScope, IgnoreCmpGeneric, AllowMoveDeref, LlvmEndBB, LlvmCtx,
        LlvmAssignmentTarget, LlvmAssignmentTargetType, LlvmPhi, std::move(CmpArgs), nullptr);
}


auto spp::asts::meta::CompilerMetaData::Restore(const bool heavy) -> void {
    auto state = std::move(_History.top()); // *DO NOT* click "convert to structured bindings" (CLion) -- LAG
    _History.pop();
    CurrentStage = state.CurrentStage;
    ReturnTypeOverloadResolverType = state.ReturnTypeOverloadResolverType;
    AssignmentTarget = state.AssignmentTarget;
    AssignmentTargetType = state.AssignmentTargetType;
    IgnoreMissingElseBranchForInference = state.IgnoreMissingElseBranchForInference;
    CaseCondition = state.CaseCondition;
    ClsSym = state.ClsSym;
    if (heavy) {
        EnclosingFunctionScope = state.EnclosingFunctionScope;
        EnclosingFunctionFlavour = state.EnclosingFunctionFlavour;
        EnclosingFunctionRetType = state.EnclosingFunctionRetType;
        EnclosingFunctionCmp = state.EnclosingFunctionCmp;
    }
    OverriddenScopeForClosure = state.OverriddenScopeForClosure;
    CurrentLambdaOuterScope = state.CurrentLambdaOuterScope;
    TargetCallFunctionPrototype = state.TargetCallFunctionPrototype;
    TargetCallWasFunctionAsync = state.TargetCallWasFunctionAsync;
    PreventAutoGeneratorResume = state.PreventAutoGeneratorResume;
    LetStatementExplicitType = state.LetStatementExplicitType;
    LetStatementValue = state.LetStatementValue;
    LetStatementFromUninitialized = state.LetStatementFromUninitialized;
    LoopDoubleCheckActive = state.LoopDoubleCheckActive;
    LoopCurrentDepth = state.LoopCurrentDepth;
    LoopCurrentAst = state.LoopCurrentAst;
    LoopReturnTypes = state.LoopReturnTypes;
    ObjectInitType = state.ObjectInitType;
    InferSource = state.InferSource;
    InferTarget = state.InferTarget;
    PostfixExpressionLhs = state.PostfixExpressionLhs;
    UnaryExpressionRhs = state.UnaryExpressionRhs;
    SkipTypeAnalysisGenericChecks = state.SkipTypeAnalysisGenericChecks;
    TypeAnalysisTypeScope = state.TypeAnalysisTypeScope;
    IgnoreCmpGeneric = state.IgnoreCmpGeneric;
    AllowMoveDeref = state.AllowMoveDeref;
    LlvmEndBB = state.LlvmEndBB;
    LlvmCtx = state.LlvmCtx;
    LlvmAssignmentTarget = state.LlvmAssignmentTarget;
    LlvmAssignmentTargetType = state.LlvmAssignmentTargetType;
    LlvmPhi = state.LlvmPhi;
    CmpArgs = std::move(state.CmpArgs);
}


auto spp::asts::meta::CompilerMetaData::Depth() const
    -> std::size_t {
    // Get the number of history items.
    return _History.size();
}

SPP_MOD_END
