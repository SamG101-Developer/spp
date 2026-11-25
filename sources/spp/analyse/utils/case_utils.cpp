module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.case_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.pattern_solver;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.object_initializer_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, VariableKind);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::analyse::utils::pattern_solver, CaseCoverage);
use_alias(spp::analyse::utils::pattern_solver, MatchPlan);
use_alias(spp::analyse::utils::pattern_solver, SubjectEdges);
use_alias(spp::asts, ArrayLiteralExplicitElementsAst);
use_alias(spp::asts, Ast);
use_alias(spp::asts, AstClone);
use_alias(spp::asts, BooleanLiteralAst);
use_alias(spp::asts, CaseExpressionBranchAst);
use_alias(spp::asts, CasePatternVariantElseAst);
use_alias(spp::asts, ObjectInitializerAst);
use_alias(spp::asts, TupleLiteralAst);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, SubjectOnMatch);
use_alias(spp::codegen, LlvmCtx);

namespace spp::analyse::utils::case_utils {
  using pattern_solver::PathStep;

  /// Join the two edges passing "branch" over after its guard ran:
  /// its patterns not matching ("before") and its guard answering
  /// false ("after_guard"). Records each edge's drops on the branch,
  /// and returns the joined state the next branch starts from.
  static auto CarryGuardMoves(
    CaseExpressionBranchAst &branch, memory_state::ScopeSnapshot const &before,
    memory_state::ScopeSnapshot const &after_guard, Vec<VariableSymbol*> const &syms,
    Map<VariableSymbol const*, Scope const*> const &owners, ScopeManager *sm,
    CompilerMetaData *meta) -> memory_state::ScopeSnapshot;

  /// The address of the value at "path" in the subject at
  /// "subject_ptr": a payload behind its variant's discriminant; a
  /// field indexed through the type holding it (an array through
  /// itself, a tuple by position, anything else through its layout's
  /// field map, which reorders fields); and a field holding a borrow
  /// read through to what it borrows.
  static auto AddressAt(
    llvm::Value * const subject_ptr, Vec<PathStep> const &path, Scope const &scope,
    LlvmCtx * const ctx)
  ->
  llvm::Value *;

  /// The comptime value at "path" in the comptime value "value": a
  /// payload is the member's value itself; a field is read out of a
  /// tuple, an array or an object. None when "value" does not have
  /// the field (it holds another member of a variant).
  static auto CompTimeValueAt(
    Ast const &value, Vec<PathStep> const &path) -> Unique<Ast>;
}

auto spp::analyse::utils::case_utils::AddressAt(
  llvm::Value * const subject_ptr, Vec<PathStep> const &path, Scope const &scope,
  LlvmCtx * const ctx)
->
llvm::Value * {
  const auto uid = "." + spp::utils::Uid();
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
  auto ptr = subject_ptr;
  for (auto const &step : path) {
    SPP_ASSERT(step.Of.Symbol != nullptr);
    const auto llvm_of_ty = codegen::GetLlvmTypeOf(
      step.Of.WithoutConvention(), ctx);
    if (step.K == PathStep::Kind::Payload) {
      ptr = codegen::GetVariantPayloadPtr(ptr, llvm_of_ty, "case.pattern.payload" + uid, ctx);
      continue;
    }

    const auto index = static_cast<std::uint32_t>(step.Index);
    if (type_predicates::IsTypeArray(step.Of, scope)) {
      ptr = ctx->Builder.CreateGEP(
        llvm_of_ty, ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
        "case.pattern.elem_ptr" + uid);
    }
    else if (std::isdigit(static_cast<unsigned char>(step.Name->Val[0]))) {
      const auto field_index = codegen::GetPhysicalFieldIndex(*step.Of.Symbol->LlvmInfo, index);
      ptr = ctx->Builder.CreateStructGEP(llvm_of_ty, ptr, field_index, "case.pattern.elem_ptr" + uid);
    }
    else {
      const auto decl_index = type_members::GetFieldIndexInType(*step.Of.Symbol, *step.Name);
      const auto field_index = codegen::GetPhysicalFieldIndex(*step.Of.Symbol->LlvmInfo, decl_index);
      ptr = ctx->Builder.CreateStructGEP(llvm_of_ty, ptr, field_index, "case.pattern.field_ptr" + uid);
    }
    if (step.Through) {
      ptr = ctx->Builder.CreateLoad(llvm::PointerType::get(*ctx->Context, 0), ptr, "case.pattern.through" + uid);
    }
  }
  return ptr;
}

auto spp::analyse::utils::case_utils::CompTimeValueAt(
  Ast const &value, Vec<PathStep> const &path) -> Unique<Ast> {
  auto out = AstClone(&value);
  for (auto const &step : path) {
    if (step.K == PathStep::Kind::Payload) { continue; }
    if (const auto tup = out->To<TupleLiteralAst>(); tup != nullptr and step.Index < tup->Elems.size()) {
      out = AstClone(tup->Elems[step.Index]);
    }
    else if (const auto arr = out->To<ArrayLiteralExplicitElementsAst>();
      arr != nullptr and step.Index < arr->Elems.size()) {
      out = AstClone(arr->Elems[step.Index]);
    }
    else if (const auto obj = out->To<ObjectInitializerAst>(); obj != nullptr) {
      out = comp_time_intrinsics::GetCompTimeAttrValue(obj, step.Name.get());
    }
    else { return nullptr; }
    if (out == nullptr) { return nullptr; }
  }
  return out;
}

auto spp::analyse::utils::case_utils::BindSubjectValue(
  pattern_solver::MatchPlan const &plan, llvm::Value *llvm_subject,
  LlvmCtx *ctx) -> llvm::Value* {
  // The hidden borrow holds the subject's address: a borrowed
  // subject is one already; any other is stored in a slot of its
  // own, once, for the checks to index.
  SPP_ASSERT(plan.Subject != nullptr and llvm_subject != nullptr);
  const auto uid = "." + spp::utils::Uid();
  auto subject_ptr = llvm_subject;
  if (not plan.SubjectHeld.IsBorrowed()) {
    subject_ptr = LlvmEntryAlloca(llvm_subject->getType(), "case.subject.value" + uid, ctx);
    ctx->Builder.CreateStore(llvm_subject, subject_ptr);
  }
  const auto slot = LlvmEntryAlloca(llvm::PointerType::get(*ctx->Context, 0), "case.subject" + uid, ctx);
  ctx->Builder.CreateStore(subject_ptr, slot);
  plan.Subject->LlvmInfo->Alloca = slot;
  return subject_ptr;
}

auto spp::analyse::utils::case_utils::EmitPatternChecks(
  pattern_solver::MatchPlan const &plan,
  Vec<pattern_solver::PatternCheck> const &checks, ScopeManager *sm,
  CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* {
  // Each check in turn, against the subject the hidden borrow
  // points at; the first that fails skips the rest, so nothing
  // reads a payload its variant does not hold.
  using pattern_solver::PatternCheck;
  if (checks.empty()) { return llvm::ConstantInt::getTrue(*ctx->Context); }
  SPP_ASSERT(plan.Subject != nullptr and plan.Subject->LlvmInfo->Alloca != nullptr);

  const auto uid = "." + spp::utils::Uid();
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto subject_ptr = ctx->Builder.CreateLoad(ptr_ty, plan.Subject->LlvmInfo->Alloca, "case.subject" + uid);

  const auto check_holds = [&](PatternCheck const &check) -> llvm::Value* {
    const auto at = AddressAt(subject_ptr, check.Path, *sm->CurrentScope, ctx);

    // A tag: the variant's discriminant is one of the members'.
    if (check.K == PatternCheck::Kind::Tag) {
      const auto variant_ty = codegen::GetLlvmTypeOf(check.Variant, ctx);
      const auto tag = codegen::LoadVariantTag(at, variant_ty, "case.pattern.tag" + uid, ctx);
      auto holds = static_cast<llvm::Value*>(nullptr);
      for (const auto member : check.Members) {
        const auto is = ctx->Builder.CreateICmpEQ(
          tag, llvm::ConstantInt::get(codegen::GetVariantTagType(ctx), member), "case.pattern.is" + uid);
        holds = holds == nullptr ? is : ctx->Builder.CreateOr(holds, is, "case.pattern.is.any" + uid);
      }
      return holds;
    }

    // A literal: its hidden borrow points at the value, then its
    // "eq" call is generated.
    const auto slot = LlvmEntryAlloca(ptr_ty, "case.pattern.value" + uid, ctx);
    ctx->Builder.CreateStore(at, slot);
    check.Value->LlvmInfo->Alloca = slot;
    return check.Call->Stage11_CodeGen(sm, meta, ctx);
  };

  // Every check but the last branches on to the next, or to "done"
  // as false; the last one's answer is the pattern's.
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "case.pattern.checked" + uid, func);
  auto failed_from = Vec<llvm::BasicBlock*>();
  for (auto k = 0uz; k + 1 < checks.size(); ++k) {
    const auto holds = check_holds(checks[k]);
    const auto next_bb = llvm::BasicBlock::Create(*ctx->Context, "case.pattern.check" + uid, func);
    failed_from.emplace_back(ctx->Builder.GetInsertBlock());
    ctx->Builder.CreateCondBr(holds, next_bb, done_bb);
    ctx->Builder.SetInsertPoint(next_bb);
  }
  const auto last = check_holds(checks.back());
  const auto last_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(done_bb);

  ctx->Builder.SetInsertPoint(done_bb);
  const auto phi = ctx->Builder.CreatePHI(
    llvm::Type::getInt1Ty(*ctx->Context), static_cast<unsigned>(failed_from.size() + 1), "case.pattern.holds" + uid);
  for (const auto bb : failed_from) { phi->addIncoming(llvm::ConstantInt::getFalse(*ctx->Context), bb); }
  phi->addIncoming(last, last_bb);
  return phi;
}

auto spp::analyse::utils::case_utils::BindSubjectCompTimeValue(
  pattern_solver::MatchPlan const &plan, Ast const &value) -> void {
  // The subject's comptime value, and each test's value at its
  // path in it.
  SPP_ASSERT(plan.Subject != nullptr);
  plan.Subject->CompTimeValue = AstClone(&value);
  for (auto const &branch : plan.Branches) {
    for (auto const &checks : branch.Checks) {
      for (auto const &check : checks) {
        if (check.Value != nullptr) { check.Value->CompTimeValue = CompTimeValueAt(value, check.Path); }
      }
    }
  }
}

auto spp::analyse::utils::case_utils::ResolvePatternChecks(
  pattern_solver::MatchPlan const &plan, Vec<pattern_solver::PatternCheck> const &checks, ScopeManager *sm,
  CompilerMetaData *meta) -> bool {
  // Every check has to hold. A tag holds when the comptime value
  // there is an object of one of the members (one of another type
  // does not; a value that is no object says nothing, so passes).
  // A literal holds when its "eq" call does; one whose value the
  // subject does not have (another member of a variant) does not.
  using pattern_solver::PatternCheck;
  for (auto const &check : checks) {
    if (check.K == PatternCheck::Kind::Tag) {
      const auto subject = plan.Subject != nullptr ? plan.Subject->CompTimeValue.get() : nullptr;
      if (subject == nullptr) { continue; }
      const auto value = CompTimeValueAt(*subject, check.Path);
      if (value == nullptr) { return false; }
      const auto obj = value->To<ObjectInitializerAst>();
      if (obj == nullptr) { continue; }
      const auto held = TypeRef::Of(*obj->Type, *sm->CurrentScope);
      const auto is_member = genex::any_of(check.MemberRefs, [&](auto const &member) {
        return type_compare::TypeEq(member, held, *sm->CurrentScope, *sm->CurrentScope);
      });
      if (not is_member) { return false; }
      continue;
    }
    if (check.Value->CompTimeValue == nullptr) { return false; }
    check.Call->EvaluateCompTime(sm, meta);
    const auto result = meta->CompTimeResult != nullptr ? meta->CompTimeResult->To<BooleanLiteralAst>() : nullptr;
    if (result == nullptr or not result->IsTrue()) { return false; }
  }
  return true;
}

auto spp::analyse::utils::case_utils::JoinBranches(
  const Span<Unique<CaseExpressionBranchAst> const> branches, ScopeManager &sm,
  CompilerMetaData *meta) -> type_join::TypeJoin {
  // A branch that diverges ("ret" from a branch) passes no value
  // back to the binding, so it is no value for the others to agree
  // with. An error points at a branch's final member.
  auto join = type_join::TypeJoin(meta->AssignmentTargetType, sm.CurrentScope, meta->AssignmentTarget.get());
  for (auto const &branch : branches) {
    auto const *const final_member = branch->Body->FinalMember();
    join.Give(
      final_member != nullptr ? *static_cast<Ast const*>(final_member) : *static_cast<Ast const*>(branch.get()),
      branch->InferType(&sm, meta), *sm.CurrentScope, branch->Body->Diverges(&sm, meta));
  }
  return join;
}

auto spp::analyse::utils::case_utils::CarryGuardMoves(
  CaseExpressionBranchAst &branch, memory_state::ScopeSnapshot const &before,
  memory_state::ScopeSnapshot const &after_guard, Vec<VariableSymbol*> const &syms,
  Map<VariableSymbol const*, Scope const*> const &owners, ScopeManager *sm,
  CompilerMetaData *meta) -> memory_state::ScopeSnapshot {
  // Two edges pass a guarded branch over: its patterns did not
  // match, so the guard never ran ("before"), or the guard ran and
  // answered false ("after_guard"). They meet at the next branch,
  // where they are joined as any paths are: a value one holds and
  // the other does not is dropped on the edge still holding it, so
  // past the branch it is held on neither (E8 on a later use). Only
  // what the guard itself changed is carried: what checking the
  // patterns did stays in the branch.
  auto const &before_guard = *branch.StateBeforeGuard();
  const auto changed = syms | genex::views::filter([&](auto const *sym) {
    const auto guard_false = memory_state::StateIn(after_guard, *sym);
    return not memory_state::SameShape(memory_state::StateIn(before_guard, *sym), guard_false);
  }) | genex::to<Vec>();
  auto drops = drop_utils::JoinPaths(
    changed, {{&before, &branch, &branch}, {&after_guard, branch.Guard.get(), branch.Guard.get()}}, owners, *sm, meta);
  drop_utils::SetDropsOn(branch, drop_utils::DropEdge::kNoMatch, std::move(drops[0]));
  drop_utils::AddDropsOn(branch, drop_utils::DropEdge::kGuardFalse, std::move(drops[1]));
  return memory_state::SnapshotSymbols(syms, *sm->CurrentScope);
}

auto spp::analyse::utils::case_utils::ValidateInconsistentMemory(
  Ast *parent, const Span<Unique<CaseExpressionBranchAst> const> branches,
  CaseCoverage const &coverage, SubjectEdges const *subject_edges,
  Vec<VariableSymbol*> const &condition_temporaries, Vec<VariableSymbol*> const &held_in_first_branch,
  ScopeManager *sm, CompilerMetaData *meta) -> Vec<drop_utils::OwnedValue> {
  // The symbols whose states are joined: those visible here,
  // and (inside a body) every one its flow has touched, so a
  // symbol hidden by a shadowing "let" is joined too. The
  // lookup can reach one symbol by more than one route.
  auto var_syms = Vec<VariableSymbol*>();
  auto var_syms_seen = Set<VariableSymbol const*>();

  // Add every symbol into the "seen" set. If it was inserted
  // into the set, ie not a duplicate, insert it into the main
  // vector too.
  for (auto sym : sm->CurrentScope->GetAllVarSymbols()) {
    if (var_syms_seen.insert(sym).second) { var_syms.emplace_back(sym); }
  }
  if (const auto flow = memory_state::FlowOf(*sm->CurrentScope); flow != nullptr) {
    for (auto const &[sym, _] : flow->States) {
      if (var_syms_seen.insert(sym).second) {
        var_syms.emplace_back(const_cast<VariableSymbol*>(sym));
      }
    }
  }
  for (auto *const sym : condition_temporaries) {
    if (var_syms_seen.insert(sym).second) { var_syms.emplace_back(sym); }
  }

  // The state each branch starts from, and the path that falls
  // through every branch, where there is one: the state before
  // the "case", with the moves of the guards passed over carried
  // on ("CarryGuardMoves"). Each branch's state is kept to be
  // joined.
  const auto owners = drop_utils::OwnersAround(*sm->CurrentScope, meta);
  auto pre_analysis = memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope);
  auto paths = Vec<Pair<CaseExpressionBranchAst*, memory_state::ScopeSnapshot>>();
  for (auto const &[i, branch] : branches | genex::views::enumerate) {
    // The branch applies its match edge to the subject.
    meta->CaseSubjectOnMatch = subject_edges != nullptr
      ? subject_edges->OnMatch[i]
      : SubjectOnMatch::kKeep;
    branch->Stage8_CheckMemory(sm, meta);
    paths.emplace_back(branch.get(), memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope));
    memory_state::RestoreSnapshot(pre_analysis);

    // Past the first branch, a boolean condition answered false, so
    // what it held only where it was true is not held.
    if (i == 0 and not held_in_first_branch.empty()) {
      for (auto *const sym : held_in_first_branch) { memory_state::StateOf(*sm->CurrentScope, *sym).MovedBy(*parent); }
      pre_analysis = memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope);
    }
    if (auto const *const after_guard = branch->StateAfterGuard(); after_guard != nullptr) {
      pre_analysis = CarryGuardMoves(*branch, pre_analysis, *after_guard, var_syms, owners, sm, meta);
    }
  }

  // On the fall-through path, the subject holds whichever
  // members no branch matched.
  paths.emplace_back(nullptr, pre_analysis);

  // The paths out of the "case": each branch that finishes (one
  // that diverges leaves no state behind to agree with; worked out
  // once per branch, as it infers the body's type), is reachable
  // (not after every value was matched), and is not an iterable
  // loop's exit "else"; and the fall-through path, where there is
  // one.
  auto diverging = Set<CaseExpressionBranchAst const*>();
  for (auto const &branch : branches) {
    if (branch->Body->Diverges(sm, meta)) { diverging.insert(branch.get()); }
  }
  auto const *const else_pattern = not branches.empty()
    ? branches.back()->Patterns[0]->To<CasePatternVariantElseAst>()
    : nullptr;
  auto const *const loop_exit_else = else_pattern != nullptr and else_pattern->MarkedForIterLoopExit()
    ? branches.back().get()
    : nullptr;
  auto leaving = Set<CaseExpressionBranchAst const*>();
  for (auto const &[i, branch] : branches | genex::views::enumerate) {
    if (not diverging.contains(branch.get()) and coverage.Reachable[i] and branch.get() != loop_exit_else) {
      leaving.insert(branch.get());
    }
  }
  const auto leaves = [&](CaseExpressionBranchAst const *branch) {
    return branch == nullptr ? coverage.FallsThrough : leaving.contains(branch);
  };

  // The first branch that finishes is the path carried on.
  const auto non_terminating_branch = genex::find_if(
    branches, [&](auto const &x) { return not diverging.contains(x.get()); });

  // A path is named by its branch, or by the "case" itself
  // where the branch was generated (a lowered "is" builds an
  // "else" with no source of its own) and so has nowhere to
  // point - or, the lowered "case" having no "case" token
  // either, by its condition, which is the "is" expression's
  // subject as written.
  auto case_named = parent->PosStart() != 0 or meta->CaseCondition == nullptr
    ? parent
    : static_cast<Ast*>(meta->CaseCondition);
  const auto path_of = [case_named](CaseExpressionBranchAst *branch) -> Ast* {
    return branch != nullptr and branch->PosStart() != 0 ? static_cast<Ast*>(branch) : case_named;
  };
  const auto first_branch = non_terminating_branch == branches.end() ? case_named : path_of(non_terminating_branch->get());
  auto const &first_path = non_terminating_branch != branches.end()
    ? paths.at(static_cast<std::size_t>(genex::iterators::distance(branches.begin(), non_terminating_branch))).second
    : paths.back().second;

  // The state after the "case" is every path's state out of it,
  // joined, each path dropping at its end (the branch's, or the
  // "case"'s for falling through) what of a scope around the "case"
  // it holds and another does not. A branch's own symbols exist only
  // in that branch.
  auto leaving_paths = Vec<Pair<CaseExpressionBranchAst*, memory_state::ScopeSnapshot const*>>();
  auto join_paths = Vec<drop_utils::JoinPath>();
  for (auto const &[branch_or_null, path] : paths) {
    if (not leaves(branch_or_null)) { continue; }
    auto const *const end = branch_or_null != nullptr ? static_cast<Ast const*>(branch_or_null) : parent;
    leaving_paths.emplace_back(branch_or_null, &path);
    join_paths.push_back({&path, path_of(branch_or_null), end});
  }
  auto path_drops = drop_utils::JoinPaths(
    var_syms, join_paths, owners, *sm, meta, drop_utils::JoinPath{&first_path, first_branch, first_branch});

  // The temporaries the condition made ("a < f() < b") are held on
  // every path out, and nothing after the "case" names them: each
  // path drops what it still holds.
  for (auto *const sym : condition_temporaries) {
    for (auto i = 0uz; i < join_paths.size(); ++i) {
      const auto state = memory_state::StateIn(*join_paths[i].State, *sym);
      if (auto value = drop_utils::OwnedOnPath(*sym, state, *sm->CurrentScope, *join_paths[i].End, *sm, meta)) {
        path_drops[i].emplace_back(std::move(*value));
      }
    }
  }
  auto fall_through_drops = Vec<drop_utils::OwnedValue>();
  for (auto i = 0uz; i < leaving_paths.size(); ++i) {
    drop_utils::SortNewestFirst(path_drops[i]);
    if (leaving_paths[i].first == nullptr) { fall_through_drops = std::move(path_drops[i]); }
    else { drop_utils::AddDropsOn(*leaving_paths[i].first, drop_utils::DropEdge::kEnd, std::move(path_drops[i])); }
  }

  // The loans held after the "case", likewise.
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  loans = first_path.Loans;
  for (auto const &[branch_or_null, path] : paths) {
    if (leaves(branch_or_null)) { memory_state::JoinLoans(loans, path.Loans, first_branch, path_of(branch_or_null)); }
  }
  return fall_through_drops;
}

auto spp::analyse::utils::case_utils::ApplySubjectOnMatch(
  ScopeManager &sm,
  CompilerMetaData const &meta)
  -> void {
  // "case x of" takes the symbol itself; "case x.inner of" takes
  // one region of it, a partial move like any other. A part the
  // patterns left unbound stays with the subject, which is left
  // partly moved for its owner to drop the rest of.
  if (meta.CaseSubject == nullptr or meta.CaseCondition == nullptr) { return; }
  if (meta.CaseSubjectOnMatch != SubjectOnMatch::kTake) { return; }
  mem_utils::TakeApart(*meta.CaseSubject, *meta.CaseCondition, *meta.CaseCondition, false, sm);
}
