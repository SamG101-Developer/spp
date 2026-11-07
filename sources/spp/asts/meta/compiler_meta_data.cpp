module;
#include <spp/macros.hpp>

module spp.asts.meta.compiler_meta_data;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;

use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::analyse::utils::type_join, TypeJoin);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::meta::CompilerMetaData::CompilerMetaData() {
  CurrentStage = CompilerStage::kNone;
  ResetContext();
  CompTimeResult = nullptr;
  LlvmGenerator = nullptr;
  LlvmGeneratorState = nullptr;
}

auto spp::asts::meta::CompilerMetaData::ResetContext() -> void {
  ReturnTypeOverloadResolverType = nullptr;
  AssignmentTarget = nullptr;
  AssignmentTargetType = nullptr;
  AssignmentTargetSymbol = nullptr;
  IgnoreMissingElseBranchForInference = false;
  CaseCondition = nullptr;
  CaseSubject = nullptr;
  CaseSubjectOnMatch = SubjectOnMatch::kKeep;
  InCompTimeValue = false;
  EnclosingFnScope = nullptr;
  EnclosingFnFlavour = nullptr;
  EnclosingFnRetType = nullptr;
  EnclosingFnSourceRetType = nullptr;
  ClosureRetJoin = nullptr;
  EnclosingFnCmp = nullptr;
  OverriddenScopeForClosure = nullptr;
  CurrentLambdaOuterScope = nullptr;
  TargetCall = nullptr;
  LetStatementExplicitType = nullptr;
  LetStatementValue = nullptr;
  LetStatementFromUninitialized = false;
  LetStatementPrecomputedValue = nullptr;
  LoopCurrentDepth = 0;
  LoopCurrentAst = nullptr;
  LoopJoins.clear();
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
  AllowBorrowedTypeArgs = false;
  AllowPackWhole = false;
  CompTimeCallSite = nullptr;
  ParameterDefaultScope = nullptr;
}

auto spp::asts::meta::CompTimeCallArgs::Swap(
  CompTimeCallArgs &other)
  -> void {
  FnArgs.swap(other.FnArgs);
  GnTypeArgs.swap(other.GnTypeArgs);
  GnCompArgs.swap(other.GnCompArgs);
  GnCompValues.swap(other.GnCompValues);
}

auto spp::asts::meta::CompTimeCallArgs::Clear() -> void {
  FnArgs.clear();
  GnTypeArgs.clear();
  GnCompArgs.clear();
  GnCompValues.clear();
}

auto spp::asts::meta::CompilerMetaData::ClearAssignmentTarget() -> void {
  AssignmentTarget = nullptr;
  AssignmentTargetType = nullptr;
}

auto spp::asts::meta::CompilerMetaData::Save(const bool heavy) -> void {
  // Reuse a parked slot at this depth if one exists, otherwise grow the pool by one. Copy-assigning into an existing
  // slot reuses its buffers (maps/vecs) rather than allocating a fresh state, and the pool is never shrunk so the
  // storage persists across cycles. `CompTimeCall` is moved out (each guard starts with none); `CompTimeResult` is not
  // tracked. The function context is only taken for a heavy guard, the one restore that puts it back.
  if (_Depth == _History.size()) { _History.emplace_back(); }
  auto &s = _History[_Depth];
  ++_Depth;

  s.CurrentStage = CurrentStage;
  s.ReturnTypeOverloadResolverType = ReturnTypeOverloadResolverType;
  s.AssignmentTarget = AssignmentTarget;
  s.AssignmentTargetType = AssignmentTargetType;
  s.AssignmentTargetSymbol = AssignmentTargetSymbol;
  s.IgnoreMissingElseBranchForInference = IgnoreMissingElseBranchForInference;
  s.CaseCondition = CaseCondition;
  s.CaseSubject = CaseSubject;
  s.CaseSubjectOnMatch = CaseSubjectOnMatch;
  s.InCompTimeValue = InCompTimeValue;
  s.WithinDeferTok = WithinDeferTok;
  s.OverriddenScopeForClosure = OverriddenScopeForClosure;
  if (heavy) {
    s.EnclosingFnScope = EnclosingFnScope;
    s.EnclosingFnFlavour = EnclosingFnFlavour;
    s.EnclosingFnRetType = EnclosingFnRetType;
    s.EnclosingFnSourceRetType = EnclosingFnSourceRetType;
    s.ClosureRetJoin = ClosureRetJoin;
    s.EnclosingFnCmp = EnclosingFnCmp;
  }
  s.CurrentLambdaOuterScope = CurrentLambdaOuterScope;
  s.TargetCall = TargetCall;
  s.LetStatementExplicitType = LetStatementExplicitType;
  s.LetStatementValue = LetStatementValue;
  s.LetStatementFromUninitialized = LetStatementFromUninitialized;
  s.LetStatementPrecomputedValue = LetStatementPrecomputedValue;
  s.LoopCurrentDepth = LoopCurrentDepth;
  s.LoopCurrentAst = LoopCurrentAst;
  s.LoopJoins = LoopJoins;
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
  CompTimeCall.Swap(s.CompTimeCall);
  CompTimeCall.Clear();
  s.IgnoreAccessModifierViolations = IgnoreAccessModifierViolations;
  s.SkipSubstitutedConstraintChecks = SkipSubstitutedConstraintChecks;
  s.AllowAbstractType = AllowAbstractType;
  s.AllowBorrowedTypeArgs = AllowBorrowedTypeArgs;
  s.AllowPackWhole = AllowPackWhole;
  s.CompTimeCallSite = CompTimeCallSite;
  s.ParameterDefaultScope = ParameterDefaultScope;
  s.LlvmGenerator = LlvmGenerator;
  s.LlvmGeneratorState = LlvmGeneratorState;
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
  AssignmentTargetSymbol = state.AssignmentTargetSymbol;
  IgnoreMissingElseBranchForInference = state.IgnoreMissingElseBranchForInference;
  CaseCondition = state.CaseCondition;
  CaseSubject = state.CaseSubject;
  CaseSubjectOnMatch = state.CaseSubjectOnMatch;
  InCompTimeValue = state.InCompTimeValue;
  WithinDeferTok = state.WithinDeferTok;
  if (heavy) {
    EnclosingFnScope = state.EnclosingFnScope;
    EnclosingFnFlavour = state.EnclosingFnFlavour;
    EnclosingFnRetType = std::move(state.EnclosingFnRetType);
    EnclosingFnSourceRetType = std::move(state.EnclosingFnSourceRetType);
    ClosureRetJoin = state.ClosureRetJoin;
    EnclosingFnCmp = state.EnclosingFnCmp;
  }
  OverriddenScopeForClosure = state.OverriddenScopeForClosure;
  CurrentLambdaOuterScope = state.CurrentLambdaOuterScope;
  TargetCall = state.TargetCall;
  LetStatementExplicitType = std::move(state.LetStatementExplicitType);
  LetStatementValue = state.LetStatementValue;
  LetStatementFromUninitialized = state.LetStatementFromUninitialized;
  LetStatementPrecomputedValue = state.LetStatementPrecomputedValue;
  LoopCurrentDepth = state.LoopCurrentDepth;
  LoopCurrentAst = state.LoopCurrentAst;
  LoopJoins = std::move(state.LoopJoins);
  SkipTypeAnalysisGnChecks = state.SkipTypeAnalysisGnChecks;
  TypeAnalysisTypeScope = state.TypeAnalysisTypeScope;
  AllowMoveDeref = state.AllowMoveDeref;
  LlvmEndBB = state.LlvmEndBB;
  LlvmWantAddress = state.LlvmWantAddress;
  LlvmAssignmentTarget = state.LlvmAssignmentTarget;
  LlvmCaseCondition = state.LlvmCaseCondition;
  LlvmPhi = state.LlvmPhi;
  LlvmLoopStack = std::move(state.LlvmLoopStack);
  CompTimeCall.Swap(state.CompTimeCall);
  state.CompTimeCall.Clear();
  // Note: CompTimeResult deliberately omitted here, allowing to pass back up.
  IgnoreAccessModifierViolations = state.IgnoreAccessModifierViolations;
  SkipSubstitutedConstraintChecks = state.SkipSubstitutedConstraintChecks;
  AllowAbstractType = state.AllowAbstractType;
  AllowBorrowedTypeArgs = state.AllowBorrowedTypeArgs;
  AllowPackWhole = state.AllowPackWhole;
  CompTimeCallSite = state.CompTimeCallSite;
  ParameterDefaultScope = state.ParameterDefaultScope;
  LlvmGenerator = state.LlvmGenerator;
  LlvmGeneratorState = state.LlvmGeneratorState;
}

spp::asts::meta::MetaGuard::MetaGuard(
  CompilerMetaData *const meta,
  const bool heavy) :
  _Meta(meta),
  _Heavy(heavy) {
  _Meta->Save(_Heavy);
}

spp::asts::meta::AllowPackWholeGuard::AllowPackWholeGuard(
  CompilerMetaData *meta, const bool allow) :
  _Meta(meta),
  _Outer(meta->AllowPackWhole) {
  _Meta->AllowPackWhole = allow;
}

spp::asts::meta::AllowPackWholeGuard::~AllowPackWholeGuard() {
  _Meta->AllowPackWhole = _Outer;
}

spp::asts::meta::MetaGuard::~MetaGuard() {
  _Meta->Restore(_Heavy);
}

spp::asts::meta::IsBindingsScope::IsBindingsScope(
  CompilerMetaData *const meta) :
  _Meta(meta),
  _Added(meta->AddedIsBindings.size()),
  _Expired(meta->ExpiredIsBindings.size()) {
}

spp::asts::meta::IsBindingsScope::~IsBindingsScope() {
  _Meta->AddedIsBindings.resize(_Added);
  _Meta->ExpiredIsBindings.resize(_Expired);
}

SPP_MOD_END
