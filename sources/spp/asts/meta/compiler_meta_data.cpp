module;
#include <spp/macros.hpp>

module spp.asts.meta.compiler_meta_data;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import ankerl;

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
    EnclosingFunctionSourceRetType = {};
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
    LlvmLoopStack = {};
    CmpResult = nullptr;
    IgnoreAccessModifierViolations = false;
    AllowAbstractType = false;
}

auto spp::asts::meta::CompilerMetaData::Save() -> void {
    // Reuse a parked slot at this depth if one exists, otherwise grow the pool by one. Copy-assigning into an existing
    // slot reuses its buffers (maps/vecs) rather than allocating a fresh state, and the pool is never shrunk so the
    // storage persists across cycles. `CmpArgs` is moved (the guarded scope rebuilds it); `CmpResult` is not tracked.
    if (_Depth == _History.size()) { _History.EmplaceBack(); }
    auto &s = _History[_Depth];
    ++_Depth;

    s.CurrentStage = CurrentStage;
    s.ReturnTypeOverloadResolverType = ReturnTypeOverloadResolverType;
    s.AssignmentTarget = AssignmentTarget;
    s.AssignmentTargetType = AssignmentTargetType;
    s.IgnoreMissingElseBranchForInference = IgnoreMissingElseBranchForInference;
    s.CaseCondition = CaseCondition;
    s.ClsSym = ClsSym;
    s.OverriddenScopeForClosure = OverriddenScopeForClosure;
    s.EnclosingFunctionScope = EnclosingFunctionScope;
    s.EnclosingFunctionFlavour = EnclosingFunctionFlavour;
    s.EnclosingFunctionRetType = EnclosingFunctionRetType;
    s.EnclosingFunctionSourceRetType = EnclosingFunctionSourceRetType;
    s.EnclosingFunctionCmp = EnclosingFunctionCmp;
    s.CurrentLambdaOuterScope = CurrentLambdaOuterScope;
    s.TargetCallFunctionPrototype = TargetCallFunctionPrototype;
    s.TargetCallWasFunctionAsync = TargetCallWasFunctionAsync;
    s.PreventAutoGeneratorResume = PreventAutoGeneratorResume;
    s.LetStatementExplicitType = LetStatementExplicitType;
    s.LetStatementValue = LetStatementValue;
    s.LetStatementFromUninitialized = LetStatementFromUninitialized;
    s.LoopDoubleCheckActive = LoopDoubleCheckActive;
    s.LoopCurrentDepth = LoopCurrentDepth;
    s.LoopCurrentAst = LoopCurrentAst;
    s.LoopReturnTypes = LoopReturnTypes;
    s.ObjectInitType = ObjectInitType;
    s.InferSource = InferSource;
    s.InferTarget = InferTarget;
    s.PostfixExpressionLhs = PostfixExpressionLhs;
    s.UnaryExpressionRhs = UnaryExpressionRhs;
    s.SkipTypeAnalysisGenericChecks = SkipTypeAnalysisGenericChecks;
    s.TypeAnalysisTypeScope = TypeAnalysisTypeScope;
    s.IgnoreCmpGeneric = IgnoreCmpGeneric;
    s.AllowMoveDeref = AllowMoveDeref;
    s.LlvmEndBB = LlvmEndBB;
    s.LlvmCtx = LlvmCtx;
    s.LlvmAssignmentTarget = LlvmAssignmentTarget;
    s.LlvmAssignmentTargetType = LlvmAssignmentTargetType;
    s.LlvmPhi = LlvmPhi;
    s.LlvmLoopStack = LlvmLoopStack;
    s.CmpArgs = std::move(CmpArgs);
    s.IgnoreAccessModifierViolations = IgnoreAccessModifierViolations;
    s.AllowAbstractType = AllowAbstractType;
}

auto spp::asts::meta::CompilerMetaData::Restore(const bool heavy) -> void {
    // Pop the top slot and move its owning fields back out. The slot is logically dead (the next Save overwrites it),
    // so stealing its shared_ptrs/maps/vecs avoids the atomic-refcount traffic and container copies that copy-assignment
    // would incur. Trivially-copyable fields are assigned directly.
    --_Depth;
    auto &state = _History[_Depth]; // *DO NOT* click "convert to structured bindings" (CLion) -- LAG
    CurrentStage = state.CurrentStage;
    ReturnTypeOverloadResolverType = std::move(state.ReturnTypeOverloadResolverType);
    AssignmentTarget = std::move(state.AssignmentTarget);
    AssignmentTargetType = std::move(state.AssignmentTargetType);
    IgnoreMissingElseBranchForInference = state.IgnoreMissingElseBranchForInference;
    CaseCondition = state.CaseCondition;
    ClsSym = state.ClsSym;
    if (heavy) {
        EnclosingFunctionScope = state.EnclosingFunctionScope;
        EnclosingFunctionFlavour = state.EnclosingFunctionFlavour;
        EnclosingFunctionRetType = std::move(state.EnclosingFunctionRetType);
        EnclosingFunctionSourceRetType = std::move(state.EnclosingFunctionSourceRetType);
        EnclosingFunctionCmp = state.EnclosingFunctionCmp;
    }
    OverriddenScopeForClosure = state.OverriddenScopeForClosure;
    CurrentLambdaOuterScope = state.CurrentLambdaOuterScope;
    TargetCallFunctionPrototype = state.TargetCallFunctionPrototype;
    TargetCallWasFunctionAsync = state.TargetCallWasFunctionAsync;
    PreventAutoGeneratorResume = state.PreventAutoGeneratorResume;
    LetStatementExplicitType = std::move(state.LetStatementExplicitType);
    LetStatementValue = state.LetStatementValue;
    LetStatementFromUninitialized = state.LetStatementFromUninitialized;
    LoopDoubleCheckActive = state.LoopDoubleCheckActive;
    LoopCurrentDepth = state.LoopCurrentDepth;
    LoopCurrentAst = state.LoopCurrentAst;
    LoopReturnTypes = std::move(state.LoopReturnTypes);
    ObjectInitType = std::move(state.ObjectInitType);
    InferSource = std::move(state.InferSource);
    InferTarget = std::move(state.InferTarget);
    PostfixExpressionLhs = state.PostfixExpressionLhs;
    UnaryExpressionRhs = state.UnaryExpressionRhs;
    SkipTypeAnalysisGenericChecks = state.SkipTypeAnalysisGenericChecks;
    TypeAnalysisTypeScope = state.TypeAnalysisTypeScope;
    IgnoreCmpGeneric = std::move(state.IgnoreCmpGeneric);
    AllowMoveDeref = state.AllowMoveDeref;
    LlvmEndBB = state.LlvmEndBB;
    LlvmCtx = state.LlvmCtx;
    LlvmAssignmentTarget = state.LlvmAssignmentTarget;
    LlvmAssignmentTargetType = state.LlvmAssignmentTargetType;
    LlvmPhi = state.LlvmPhi;
    LlvmLoopStack = std::move(state.LlvmLoopStack);
    CmpArgs = std::move(state.CmpArgs);
    // CmpResult = std::move(state.CmpResult);
    IgnoreAccessModifierViolations = state.IgnoreAccessModifierViolations;
    AllowAbstractType = state.AllowAbstractType;
}

auto spp::asts::meta::CompilerMetaData::Depth() const
    -> std::size_t {
    // Get the number of live history items.
    return _Depth;
}

SPP_MOD_END
