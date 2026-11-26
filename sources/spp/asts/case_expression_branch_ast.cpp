module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_expression_branch_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.pattern_solver;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_drop;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeBlockName);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::utils::pattern_solver, MatchPlan);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
  decltype(Op) &&op,
  decltype(Patterns) &&patterns,
  decltype(Guard) &&guard,
  decltype(Body) &&body) :
  Op(std::move(op)),
  Patterns(std::move(patterns)),
  Guard(std::move(guard)),
  Body(std::move(body)) {
  // Default the body to empty.
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Body);
}

spp::asts::CaseExpressionBranchAst::~CaseExpressionBranchAst() = default;

auto spp::asts::CaseExpressionBranchAst::PosStart() const -> std::size_t {
  // Use the op or first pattern
  return Op ? Op->PosStart() : Patterns.front()->PosStart();
}

auto spp::asts::CaseExpressionBranchAst::PosEnd() const -> std::size_t {
  // Use the final pattern.
  return Patterns.back()->PosEnd();
}

auto spp::asts::CaseExpressionBranchAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, carrying over the
  // comparisons stage 7 built: a clone taken after it (a
  // "case" as a method's receiver, passed on as "self") is
  // generated without being analysed again.
  auto cloned = MakeUnique<CaseExpressionBranchAst>(
    AstClone(Op),
    AstCloneVec(Patterns),
    AstClone(Guard),
    AstClone(Body));
  cloned->_MappedPatFns = AstCloneVec(_MappedPatFns);
  cloned->_KeepsTemporary = _KeepsTemporary;
  cloned->_DropsPatternBindings = _DropsPatternBindings;
  cloned->_Drops = _Drops;
  return cloned;
}

auto spp::asts::CaseExpressionBranchAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Op).append(" ");
  SPP_STRING_EXTEND(Patterns, ", ");
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_APPEND(Guard).append(Guard ? " " : "");
  SPP_STRING_APPEND(Body);
  SPP_STRING_END;
}

auto spp::asts::CaseExpressionBranchAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Create a scope for the branch - this is where
  // destructures of patterns will reside.
  auto scope_name = ScopeBlockName::FromParts(
    "case-branch", {}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);

  // Analyse the patterns, ensuring comparison methods
  // exist is needed.
  for (auto const &p : Patterns) {
    p->Stage7_AnalyseSemantics(sm, meta);
  }

  // Build the comparison the branch actually tests, over the
  // subject's hidden borrow (so the condition is not evaluated
  // again) and the operand. This is to retained, rather than
  // needing to rebuild at codegen time. Only needed for "case
  // ... of".
  _MappedPatFns = Vec<Unique<BinaryExpressionAst>>(Patterns.size());
  if (Op != nullptr and Op->TokenType != lex::SppTokenType::KW_IS) {
    for (auto const &[i, p] : Patterns | genex::views::enumerate) {
      const auto pe = p->To<CasePatternVariantExpressionAst>();
      if (pe == nullptr) { continue; }

      auto subject = MakeUnique<IdentifierAst>(
        meta->CaseCondition->PosStart(), pattern_solver::SubjectValueName());
      _MappedPatFns[i] = MakeUnique<BinaryExpressionAst>(std::move(subject), AstClone(Op), AstClone(pe->Expr));
      _MappedPatFns[i]->Stage7_AnalyseSemantics(sm, meta);
    }
  }

  // Analyse the guard and body: what the guard bound where it is true
  // holds in the body.
  if (Guard) {
    Guard->Stage7_AnalyseSemantics(sm, meta);
    Body->SetBoundOnEntry(analyse::utils::expr_utils::BoundWhenTrue(*Guard->Expr));
  }
  Body->Stage7_AnalyseSemantics(sm, meta);

  // Exit the scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Move into the branch's scope.
  sm->MoveToNextScope();

  // Check the patterns and guard, then the match edge they
  // lead to (what the branch does with the "case" subject),
  // then the body.
  IMPORT_UTILS;
  for (auto const &p : Patterns) { p->Stage8_CheckMemory(sm, meta); }
  _StateBeforeGuard = nullptr;
  _StateAfterGuard = nullptr;
  _Drops.BadMatch = {};
  _Drops.BadGuard = {};
  if (Guard) {
    // The guard's temporaries live on into the body (its "is"
    // bindings view them), so where it answers false, what they
    // hold is dropped on that edge.
    _StateBeforeGuard = MakeShared<memory_state::ScopeSnapshot>(memory_state::SnapshotFlow(*sm->CurrentScope));
    const auto guard = drop_utils::TemporaryRegion(*sm->CurrentScope);
    BinaryExpressionAst::MarkCondition(*Guard->Expr);
    Guard->Stage8_CheckMemory(sm, meta);
    const auto held_when_true = BinaryExpressionAst::HeldWhenTrue(*Guard->Expr);
    _Drops.BadGuard = guard.Held(*Guard, *sm, meta, held_when_true);
    _StateAfterGuard = MakeShared<memory_state::ScopeSnapshot>(memory_state::SnapshotFlow(*sm->CurrentScope));
  }
  case_utils::ApplySubjectOnMatch(*sm, *meta);
  Body->Stage8_CheckMemory(sm, meta);

  // The bindings end with the branch, so what they still own is
  // dropped at its end, unless it never gets there. (An "is" in the
  // "case" condition binds views of its subject, owning nothing.)
  _Drops.End = {};
  if (_DropsPatternBindings and not Body->Diverges(sm, meta)) {
    auto const &end = Body->TokR != nullptr ? static_cast<Ast const&>(*Body->TokR) : static_cast<Ast const&>(*this);
    _Drops.End = drop_utils::EndRegion(*sm->CurrentScope, end, *sm, meta);
  }

  // The pattern's bindings end with the branch, and so do
  // the loans they hold.
  drop_utils::ReleaseOwnedLoans(*sm->CurrentScope);

  // Move out of the branch's scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  // Generate the branch architecture. Start by defining blocks
  // for the branch's "body" and "next" (after body) zones.
  sm->MoveToNextScope();
  const auto uid = "." + Uid();
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto body_bb = llvm::BasicBlock::Create(
    *ctx->Context, "case.branch.body" + uid, func);
  const auto next_bb = llvm::BasicBlock::Create(
    *ctx->Context, "case.branch.next" + uid, func);

  // Get the pattern condition (destructuring bindings checks
  // happen as part of this - always safe to compute eagerly,
  // since they are just loads/compares over memory that is
  // always in-bounds regardless of which variant is selected).
  const auto cond = _CodegenCombinePatterns(sm, meta, ctx);

  // A temporary subject this branch keeps is held from where its patterns matched: a way out of its guard ("?")
  // drops it with the other temporaries, as a way out of its body does.
  const auto temporaries = codegen::TemporaryDepth(ctx);
  const auto keeps_temporary = _KeepsTemporary and meta->LlvmCaseTemporary != nullptr;
  if (keeps_temporary) { codegen::PushTemporary(meta->LlvmCaseTemporary, *meta->LlvmCaseTemporaryType, {}, ctx); }

  if (Guard) {
    // The guard is user-written and may assume the pattern
    // actually matched (eg. "is Some(v) and v.foo()" reading
    // "v" as a genuine payload) or have observable side
    // effects, so it must only run once the pattern is known
    // to have matched. Branch first, and only evaluate the
    // guard in the block reached exclusively when "cond" was
    // true, rather than eagerly ANDing it into "cond" and
    // evaluating it unconditionally.
    const auto guard_bb = llvm::BasicBlock::Create(
      *ctx->Context, "case.branch.guard" + uid, func);
    const auto no_match_bb = _EdgeToNext(_Drops.BadMatch, "case.branch.no_match" + uid, next_bb, sm, meta, ctx);
    ctx->Builder.CreateCondBr(cond, guard_bb, no_match_bb);
    // The unnamed temporaries the guard makes exist only where it
    // ran: they are dropped at its end, before either edge, its value
    // being a "Bool" that borrows none of them. Its "$" ones live on
    // into the body, and are dropped on the false edge.
    ctx->Builder.SetInsertPoint(guard_bb);
    const auto guard_temporaries = codegen::TemporaryDepth(ctx);
    const auto guard_cond = Guard->Stage11_CodeGen(sm, meta, ctx);
    codegen::EndTemporaryRegion(guard_temporaries, *sm->CurrentScope, {}, Guard.get(), sm, meta, ctx);
    const auto guard_end_bb = ctx->Builder.GetInsertBlock();
    const auto false_bb = _EdgeToNext(_Drops.BadGuard, "case.branch.guard_false" + uid, next_bb, sm, meta, ctx);
    ctx->Builder.SetInsertPoint(guard_end_bb);
    ctx->Builder.CreateCondBr(guard_cond, body_bb, false_bb);
  }
  else {
    // Otherwise, we have no guard in place, so just branch to
    // the "body" if true, and the "next" zone if false.
    ctx->Builder.CreateCondBr(cond, body_bb, next_bb);
  }
  ctx->Builder.SetInsertPoint(body_bb);

  // Generate the body. As this is an expression, an llvm value
  // is returned, which can then be used in the PHI node system.
  auto llvm_val = Body->Stage11_CodeGen(sm, meta, ctx);
  const auto incoming_bb = ctx->Builder.GetInsertBlock();

  // Sometimes, a type is returned from a branch that is part of
  // the variant type on the lhs. For example, a Opt[T] might
  // receive a Some[T] in one branch, and a None in another. In
  // this case the member value has to be tagged and copied into
  // the variant's payload (a bit-cast cannot express that).
  // A named function is likewise built into the function value
  // the target's type asks for, one branch at a time, as each
  // branch may name a different one. The "case" sets the type
  // only when it yields a value, assigned or not ("f(case ..)").
  if (meta->AssignmentTargetType != nullptr and llvm_val != nullptr) {
    const auto target_ref = TypeRef::Of(*meta->AssignmentTargetType, *sm->CurrentScope);
    const auto body_ref = Body->InferTypeRef(sm, meta);
    llvm_val = codegen::CoerceToSlot(llvm_val, target_ref, body_ref, *sm, "case.branch.variant" + uid, ctx);
  }

  // A body that never finishes has no value to hand on, and
  // no edge back to the "case"'s end.
  const auto body_is_never = Body->Diverges(sm, meta);

  // Add the value generated from the branch's body into the PHI
  // node of the "meta" context. This will then be pulled by the
  // parent "case" AST. Given the branch doesn't terminate (return),
  // we branch back to the "end" block of the "case" expression.
  if (not incoming_bb->hasTerminator() and body_is_never) {
    ctx->Builder.CreateUnreachable();
  }
  else if (not incoming_bb->hasTerminator()) {
    // The bindings still owned are dropped once the body's value is made; the phi's edge comes from wherever
    // dropping them left the builder.
    codegen::EmitTemporaryDrops(temporaries, sm, meta, ctx);
    codegen::EmitOuterDrops(_Drops.End, *this, sm, meta, ctx);
    if (meta->LlvmPhi != nullptr) { meta->LlvmPhi->addIncoming(llvm_val, ctx->Builder.GetInsertBlock()); }
    ctx->Builder.CreateBr(meta->LlvmEndBB);
  }

  // Set the current point to the "next" zone, so that the next
  // branch can be added in the right place. Move out of the
  // branch's scope.
  codegen::PopTemporaries(temporaries, ctx);
  ctx->Builder.SetInsertPoint(next_bb);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::CaseExpressionBranchAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Combine the case expression with the pattern to determine
  // if this branch should be taken, at compile-time.
  sm->MoveToNextScope();
  const auto tests_condition_directly =
    Op == nullptr
    and not Patterns.empty()
    and Patterns[0]->To<CasePatternVariantExpressionAst>() != nullptr;

  SPP_ASSERT(_MappedPatFns.size() == Patterns.size());
  for (auto const &[i, pattern] : Patterns | genex::views::enumerate) {
    auto *tested = tests_condition_directly
      ? static_cast<Ast*>(meta->CaseCondition)
      : _MappedPatFns[i] != nullptr
      ? static_cast<Ast*>(_MappedPatFns[i].get())
      : static_cast<Ast*>(pattern.get());

    // Determine if this branch is not a match (false): the checks
    // the plan says the pattern makes, before the pattern binds
    // anything (a binding reads a payload only its tag says is
    // there), and then the pattern itself.
    const auto checks_hold = tested != pattern.get()
      or case_utils::ResolvePatternChecks(
        *_CasePlan, _CasePlan->Branches[_CaseIndex].Checks[i], sm, meta);
    if (checks_hold) { tested->EvaluateCompTime(sm, meta); }
    const auto cmp_pat_bool = checks_hold and meta->CompTimeResult != nullptr
      ? meta->CompTimeResult->To<BooleanLiteralAst>()
      : nullptr;
    if (cmp_pat_bool == nullptr or not cmp_pat_bool->IsTrue()) {
      ignore = sm->SkipSubtree(sm->CurrentScope);
      continue;
    }

    // Check with the branch guard if it exists.
    if (Guard != nullptr) {
      Guard->EvaluateCompTime(sm, meta);
      const auto cmp_guard_bool = meta->CompTimeResult ? meta->CompTimeResult->To<BooleanLiteralAst>() : nullptr;
      if (not cmp_guard_bool or not cmp_guard_bool->IsTrue()) {
        ignore = sm->SkipSubtree(sm->CurrentScope);
        continue;
      }
    }

    // At this point, the correct branch has been identified, so resolve the body.
    Body->EvaluateCompTime(sm, meta);
    sm->MoveOutOfCurrentScope();
    return;
  }

  // None of the patterns on this branch matched.
  meta->CompTimeResult = nullptr;
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::UsePlan(
  MatchPlan const *plan, const std::size_t index) -> void {
  _CasePlan = plan;
  _CaseIndex = index;
}

auto spp::asts::CaseExpressionBranchAst::SetDropsBindings(
  const bool patterns) -> void {
  _DropsPatternBindings = patterns;
}

auto spp::asts::CaseExpressionBranchAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Forward type inference to the body.
  return Body->InferType(sm, meta);
}

auto spp::asts::CaseExpressionBranchAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return Body->InferTypeRef(sm, meta);
}

auto spp::asts::CaseExpressionBranchAst::_CodegenCombinePatterns(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) const -> llvm::Value* {
  // The "case c { ... }" form desugars to a branch with no
  // operator and a synthesised "true" pattern, meaning "taken
  // when c is true". Skip the pattern codegen.
  const auto tests_condition_directly =
    Op == nullptr
    and not Patterns.empty()
    and Patterns[0]->To<CasePatternVariantExpressionAst>() != nullptr;

  if (tests_condition_directly) {
    return meta->LlvmCaseCondition;
  }

  // Reuse either the generated pattern combinations, or the normal
  // pattern codegen (its bindings) and then the checks the plan says
  // it makes (tags, literals).
  IMPORT_UTILS;
  SPP_ASSERT(_MappedPatFns.size() == Patterns.size());
  const auto codegen_pattern = [&](const std::size_t i) -> llvm::Value* {
    if (_MappedPatFns[i] != nullptr) {
      return _MappedPatFns[i]->Stage11_CodeGen(sm, meta, ctx);
    }
    const auto shape = Patterns[i]->Stage11_CodeGen(sm, meta, ctx);
    if (_CasePlan->Branches[_CaseIndex].Checks[i].empty()) { return shape; }
    const auto checks = case_utils::EmitPatternChecks(
      *_CasePlan, _CasePlan->Branches[_CaseIndex].Checks[i], sm, meta, ctx);
    return ctx->Builder.CreateAnd(shape, checks);
  };

  // If there is only one pattern, generate its condition directly.
  // Otherwise, collect all the pattern conditions and combine them
  // with OR. The guard (if any) is deliberately not folded in here
  // because otherwise the inner pattern codegen is executed despite
  // a false guard. The guard is executed before this from the case
  // expression ast.
  auto llvm_combined_pattern = codegen_pattern(0);
  for (auto i = 1uz; i < Patterns.size(); ++i) {
    llvm_combined_pattern = ctx->Builder.CreateOr(llvm_combined_pattern, codegen_pattern(i));
  }
  return llvm_combined_pattern;
}

auto spp::asts::CaseExpressionBranchAst::StateBeforeGuard() const
  -> analyse::utils::memory_state::ScopeSnapshot const* {
  return _StateBeforeGuard.get();
}

auto spp::asts::CaseExpressionBranchAst::StateAfterGuard() const
  -> analyse::utils::memory_state::ScopeSnapshot const* {
  return _StateAfterGuard.get();
}

auto spp::asts::CaseExpressionBranchAst::_EdgeToNext(
  Vec<analyse::utils::drop_utils::OwnedValue> const &drops, Str const &name, llvm::BasicBlock *next_bb,
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::BasicBlock* {
  // An edge passing this branch over that drops nothing goes
  // straight on; one that does drops in a block of its own. The
  // insert point is left where it was.
  if (drops.empty()) { return next_bb; }
  const auto here = ctx->Builder.GetInsertBlock();
  const auto edge_bb = llvm::BasicBlock::Create(*ctx->Context, name, here->getParent());
  ctx->Builder.SetInsertPoint(edge_bb);
  codegen::EmitOuterDrops(drops, *this, sm, meta, ctx);
  ctx->Builder.CreateBr(next_bb);
  ctx->Builder.SetInsertPoint(here);
  return edge_bb;
}

auto spp::asts::CaseExpressionBranchAst::SetKeepsTemporary(
  const bool keeps) -> void {
  _KeepsTemporary = keeps;
}

auto spp::asts::CaseExpressionBranchAst::KeepsTemporary() const -> bool {
  return _KeepsTemporary;
}

SPP_MOD_END
