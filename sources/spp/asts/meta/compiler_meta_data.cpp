module;
#include <spp/macros.hpp>

module spp.asts.meta.compiler_meta_data;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;

SPP_MOD_BEGIN
CompilerMetaData::CompilerMetaData() {
  CurrentStage = CompilerStage::kNone;
  ResetContext();
  CompTimeResult = nullptr;
  LlvmGenerator = nullptr;
  LlvmGeneratorState = nullptr;
}

auto CompilerMetaData::ResetContext() -> void {
  ReturnTypeOverloadResolverType = nullptr;
  AssignmentTarget = nullptr;
  AssignmentTargetType = nullptr;
  IgnoreMissingElseBranchForInference = false;
  CaseCondition = nullptr;
  CaseConsumedSubjects.Clear();
  EnclosingFnScope = nullptr;
  EnclosingFnFlavour = nullptr;
  EnclosingFnRetType = {};
  EnclosingFnSourceRetType = {};
  EnclosingFnCmp = nullptr;
  OverriddenScopeForClosure = nullptr;
  CurrentLambdaOuterScope = nullptr;
  TargetCallFnPrototype = nullptr;
  TargetCallWasFnAsync = false;
  LetStatementExplicitType = nullptr;
  LetStatementValue = nullptr;
  LetStatementFromUninitialized = false;
  LetStatementPrecomputedValue = nullptr;
  LoopCurrentDepth = 0;
  LoopCurrentAst = nullptr;
  LoopReturnTypes = MakeShared<Map<std::size_t, Tup<ExpressionAst*, Shared<TypeAst>, Scope*>>>();
  ObjectInitType = nullptr;
  PostfixExpressionLhs = nullptr;
  UnaryExpressionRhs = nullptr;
  SkipTypeAnalysisGnChecks = false;
  TypeAnalysisTypeScope = nullptr;
  AllowMoveDeref = false;
  LlvmEndBB = nullptr;
  LlvmWantAddress = false;
  LlvmAssignmentTarget = nullptr;
  LlvmCaseCondition = nullptr;
  LlvmPhi = nullptr;
  LlvmLoopStack = {};
  IgnoreAccessModifierViolations = false;
  SkipSubstitutedConstraintChecks = false;
  AllowAbstractType = false;
  CompTimeCallSite = nullptr;
  CompTimeCallSiteScope = nullptr;
}

auto CompilerMetaData::Save() -> void {
  // Reuse a parked slot at this depth if one exists, otherwise grow the pool by one. Copy-assigning into an existing
  // slot reuses its buffers (maps/vecs) rather than allocating a fresh state, and the pool is never shrunk so the
  // storage persists across cycles. `CompTimeArgs` is moved (the guarded scope rebuilds it); `CompTimeResult` is not
  // tracked.
  if (_Depth == _History.size()) { _History.EmplaceBack(); }
  auto &s = _History[_Depth];
  ++_Depth;

  s.CurrentStage = CurrentStage;
  s.ReturnTypeOverloadResolverType = ReturnTypeOverloadResolverType;
  s.AssignmentTarget = AssignmentTarget;
  s.AssignmentTargetType = AssignmentTargetType;
  s.IgnoreMissingElseBranchForInference = IgnoreMissingElseBranchForInference;
  s.CaseCondition = CaseCondition;
  s.CaseConsumedSubjects = CaseConsumedSubjects;
  s.WithinDeferTok = WithinDeferTok;
  s.OverriddenScopeForClosure = OverriddenScopeForClosure;
  s.EnclosingFnScope = EnclosingFnScope;
  s.EnclosingFnFlavour = EnclosingFnFlavour;
  s.EnclosingFnRetType = EnclosingFnRetType;
  s.EnclosingFnSourceRetType = EnclosingFnSourceRetType;
  s.EnclosingFnCmp = EnclosingFnCmp;
  s.CurrentLambdaOuterScope = CurrentLambdaOuterScope;
  s.TargetCallFnPrototype = TargetCallFnPrototype;
  s.TargetCallWasFnAsync = TargetCallWasFnAsync;
  s.LetStatementExplicitType = LetStatementExplicitType;
  s.LetStatementValue = LetStatementValue;
  s.LetStatementFromUninitialized = LetStatementFromUninitialized;
  s.LetStatementPrecomputedValue = LetStatementPrecomputedValue;
  s.LoopCurrentDepth = LoopCurrentDepth;
  s.LoopCurrentAst = LoopCurrentAst;
  s.LoopReturnTypes = LoopReturnTypes;
  s.ObjectInitType = ObjectInitType;
  s.PostfixExpressionLhs = PostfixExpressionLhs;
  s.UnaryExpressionRhs = UnaryExpressionRhs;
  s.SkipTypeAnalysisGnChecks = SkipTypeAnalysisGnChecks;
  s.TypeAnalysisTypeScope = TypeAnalysisTypeScope;
  s.AllowMoveDeref = AllowMoveDeref;
  s.LlvmEndBB = LlvmEndBB;
  s.LlvmWantAddress = LlvmWantAddress;
  s.LlvmAssignmentTarget = LlvmAssignmentTarget;
  s.LlvmCaseCondition = LlvmCaseCondition;
  s.LlvmPhi = LlvmPhi;
  s.LlvmLoopStack = LlvmLoopStack;
  // Swapped rather than move-assigned. Move-assignment destroys what the parked slot already holds and then takes
  // this one's buffers, leaving both sides to allocate again next cycle - which is exactly what the pool is meant to
  // avoid. Swapping hands the slot the live contents and hands this side the slot's dead ones, and clearing those
  // frees the same objects at the same point a move-assignment would have, keeping the allocation on both sides.
  CompTimeArgs.swap(s.CompTimeArgs);
  CompTimeArgs.clear();
  CompTimeGnTypeArgs.Swap(s.CompTimeGnTypeArgs);
  CompTimeGnTypeArgs.Clear();
  CompTimeGnCompArgs.Swap(s.CompTimeGnCompArgs);
  CompTimeGnCompArgs.Clear();
  s.IgnoreAccessModifierViolations = IgnoreAccessModifierViolations;
  s.SkipSubstitutedConstraintChecks = SkipSubstitutedConstraintChecks;
  s.AllowAbstractType = AllowAbstractType;
  s.CompTimeCallSite = CompTimeCallSite;
  s.CompTimeCallSiteScope = CompTimeCallSiteScope;
  s.LlvmGenerator = LlvmGenerator;
  s.LlvmGeneratorState = LlvmGeneratorState;
}

auto CompilerMetaData::Restore(const bool heavy) -> void {
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
  CaseConsumedSubjects = std::move(state.CaseConsumedSubjects);
  WithinDeferTok = state.WithinDeferTok;
  if (heavy) {
    EnclosingFnScope = state.EnclosingFnScope;
    EnclosingFnFlavour = state.EnclosingFnFlavour;
    EnclosingFnRetType = std::move(state.EnclosingFnRetType);
    EnclosingFnSourceRetType = std::move(state.EnclosingFnSourceRetType);
    EnclosingFnCmp = state.EnclosingFnCmp;
  }
  OverriddenScopeForClosure = state.OverriddenScopeForClosure;
  CurrentLambdaOuterScope = state.CurrentLambdaOuterScope;
  TargetCallFnPrototype = state.TargetCallFnPrototype;
  TargetCallWasFnAsync = state.TargetCallWasFnAsync;
  LetStatementExplicitType = std::move(state.LetStatementExplicitType);
  LetStatementValue = state.LetStatementValue;
  LetStatementFromUninitialized = state.LetStatementFromUninitialized;
  LetStatementPrecomputedValue = state.LetStatementPrecomputedValue;
  LoopCurrentDepth = state.LoopCurrentDepth;
  LoopCurrentAst = state.LoopCurrentAst;
  LoopReturnTypes = std::move(state.LoopReturnTypes);
  ObjectInitType = std::move(state.ObjectInitType);
  PostfixExpressionLhs = state.PostfixExpressionLhs;
  UnaryExpressionRhs = state.UnaryExpressionRhs;
  SkipTypeAnalysisGnChecks = state.SkipTypeAnalysisGnChecks;
  TypeAnalysisTypeScope = state.TypeAnalysisTypeScope;
  AllowMoveDeref = state.AllowMoveDeref;
  LlvmEndBB = state.LlvmEndBB;
  LlvmWantAddress = state.LlvmWantAddress;
  LlvmAssignmentTarget = state.LlvmAssignmentTarget;
  LlvmCaseCondition = state.LlvmCaseCondition;
  LlvmPhi = state.LlvmPhi;
  LlvmLoopStack = std::move(state.LlvmLoopStack);
  CompTimeArgs.swap(state.CompTimeArgs);
  state.CompTimeArgs.clear();
  CompTimeGnTypeArgs.Swap(state.CompTimeGnTypeArgs);
  state.CompTimeGnTypeArgs.Clear();
  CompTimeGnCompArgs.Swap(state.CompTimeGnCompArgs);
  state.CompTimeGnCompArgs.Clear();
  // Note: CompTimeResult deliberately omitted here, allowing to pass back up.
  IgnoreAccessModifierViolations = state.IgnoreAccessModifierViolations;
  SkipSubstitutedConstraintChecks = state.SkipSubstitutedConstraintChecks;
  AllowAbstractType = state.AllowAbstractType;
  CompTimeCallSite = state.CompTimeCallSite;
  CompTimeCallSiteScope = state.CompTimeCallSiteScope;
  LlvmGenerator = state.LlvmGenerator;
  LlvmGeneratorState = state.LlvmGeneratorState;
}

auto CompilerMetaData::Depth() const
  -> std::size_t {
  // Get the number of live history items.
  return _Depth;
}

MetaGuard::MetaGuard(
  CompilerMetaData *const meta,
  const bool heavy) :
  _Meta(meta),
  _Heavy(heavy) {
  _Meta->Save();
}

MetaGuard::~MetaGuard() {
  _Meta->Restore(_Heavy);
}

SPP_MOD_END
