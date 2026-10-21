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
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.pattern_solver;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.object_initializer_ast;
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

namespace spp::analyse::utils::case_utils {
  namespace {
    using pattern_solver::PathStep;

    /// The address of the value at "path" in the subject at
    /// "subject_ptr": a payload behind its variant's discriminant; a
    /// field indexed through the type holding it (an array through
    /// itself, a tuple by position, anything else through its layout's
    /// field map, which reorders fields); and a field holding a borrow
    /// read through to what it borrows.
    auto AddressAt(
      llvm::Value * const subject_ptr, Vec<PathStep> const &path, Scope const &scope,
      LlvmCtx * const ctx)
    ->
    llvm::Value * {
      const auto uid = "." + spp::utils::Uid();
      const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
      auto ptr = subject_ptr;
      for (auto const &step : path) {
        SPP_ASSERT(step.Of.Symbol != nullptr);
        const auto llvm_of_ty = codegen::GetLlvmType(*step.Of.Symbol, ctx);
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
          ptr = ctx->Builder.CreateStructGEP(llvm_of_ty, ptr, index, "case.pattern.elem_ptr" + uid);
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

    /// The comptime value at "path" in the comptime value "value": a
    /// payload is the member's value itself; a field is read out of a
    /// tuple, an array or an object. None when "value" does not have
    /// the field (it holds another member of a variant).
    auto CompTimeValueAt(
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
  }
}

auto spp::analyse::utils::case_utils::BindSubjectValue(
  pattern_solver::MatchPlan const &plan, llvm::Value *llvm_subject,
  LlvmCtx *ctx) -> void {
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
    check.Call->Stage9_CompTimeResolve(sm, meta);
    const auto result = meta->CompTimeResult != nullptr ? meta->CompTimeResult->To<BooleanLiteralAst>() : nullptr;
    if (result == nullptr or not result->IsTrue()) { return false; }
  }
  return true;
}

auto spp::analyse::utils::case_utils::ValidateInconsistentTypes(
  Vec<CaseExpressionBranchAst*> const &branches, ScopeManager &sm,
  CompilerMetaData *meta) -> Tup<Pair<Ast*, Shared<TypeAst>>, Vec<Pair<Ast*, Shared<TypeAst>>>> {
  //
  using errors::SppTypeMismatchError;

  // Collect type information for each branch, pairing the
  // branch with its inferred type.
  auto branches_type_info = branches
    | genex::views::transform([&sm, meta](auto *x) { return MakePair(x, x->InferType(&sm, meta)); })
    | genex::to<Vec>();

  // The valued branches are branches that are non-terminating.
  // This is because "ret" from a branch doesn't pass a value
  // back to the binding, so shouldn't be considered for type
  // checking.
  auto valued_branches_type_info = branches_type_info
    | genex::views::remove_if([&sm, meta](auto const &x) { return expr_utils::Diverges(*x.first->Body, &sm, meta); })
    | genex::to<Vec>();

  // When every branch diverges, the ones typed "!" stand for the
  // case (it fits whatever it is assigned to); a "ret" branch's
  // own type is "Void", which the case never produces. No value
  // leaves such a case, so there is nothing for its branches to
  // agree on with what it is assigned to.
  const auto every_branch_diverges = valued_branches_type_info.empty();
  if (valued_branches_type_info.empty()) {
    valued_branches_type_info = branches_type_info
      | genex::views::filter([](auto const &x) { return x.second->IsNeverType(); })
      | genex::to<Vec>();
  }
  if (valued_branches_type_info.empty()) { valued_branches_type_info = branches_type_info; }

  // Filter the branch types down to variant types for custom
  // analysis.
  auto variant_branches_type_info = valued_branches_type_info
    | genex::views::filter([&sm](auto &&x) {
      return type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*x.second, *sm.CurrentScope), *sm.CurrentScope);
    })
    | genex::to<Vec>();

  // Set the master branch type to the first branch that has a
  // value: a branch that diverges ("!") has none for the others
  // to agree with, and only a case whose every branch diverges
  // is "!" itself. This is the default and may be subsequently
  // changed. Override it if an assignment type is given.
  auto master_branch_type_info = MakePair<CaseExpressionBranchAst*, Shared<TypeAst>>(nullptr, nullptr);
  for (auto const &[branch, type] : valued_branches_type_info) {
    if (not type->IsNeverType()) {
      master_branch_type_info = MakePair(branch, type);
      break;
    }
  }
  if (master_branch_type_info.first == nullptr and not valued_branches_type_info.empty()) {
    master_branch_type_info = MakePair(valued_branches_type_info[0].first, valued_branches_type_info[0].second);
  }
  if (meta->AssignmentTargetType != nullptr) {
    master_branch_type_info = MakePair(nullptr, meta->AssignmentTargetType);
  }

  // Otherwise, if there are variant branches, use the most
  // variant type as the master branch type.
  else if (not variant_branches_type_info.empty()) {
    auto most_inner_types = 0uz;
    for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
      const auto variant_size = type_members::VariantMemberRefs(
        TypeRef::Of(*variant_type, *sm.CurrentScope), *sm.CurrentScope).size();
      if (variant_size > most_inner_types) {
        master_branch_type_info = {variant_branch, variant_type};
        most_inner_types = variant_size;
      }
    }
  }

  // Remove the master branch pointer from the list of remaining
  // branch types and check all types match. A "!" branch fits
  // the master type like any value would, so "TypeEq" drops it.
  auto mismatch_branches_type_info = valued_branches_type_info
    | genex::views::remove_if([&](auto const &x) {
      return x.first == master_branch_type_info.first;
    })
    | genex::views::remove_if([&](auto const &x) {
      return type_compare::Assignable(
        TypeRef::Of(*master_branch_type_info.second, *sm.CurrentScope),
        TypeRef::Of(*x.second, *sm.CurrentScope),
        *sm.CurrentScope, *sm.CurrentScope);
    })
    | genex::to<Vec>();

  if (not every_branch_diverges and not mismatch_branches_type_info.empty()) {
    const auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
    const auto [master_branch, master_branch_type] = master_branch_type_info;
    const auto final_member = master_branch != nullptr
      ? master_branch->Body->FinalMember()
      : meta->AssignmentTarget != nullptr
      ? static_cast<Ast*>(meta->AssignmentTarget.get())
      : mismatch_branch->Body->FinalMember();
    Raise<SppTypeMismatchError>(
      ERR_ARGS(*final_member, *master_branch_type, *mismatch_branch->Body->FinalMember(), *mismatch_branch_type));
  }

  // The `master_branch_type_info.first` is deliberately null when an
  // assignment target type drove the master type (see above); calling
  // `To<>()` through that null pointer is UB, so guard it and keep
  // the null.
  const auto cast_master_branch_type_info = MakePair(
    master_branch_type_info.first ? master_branch_type_info.first->template ToUnchecked<Ast>() : nullptr,
    master_branch_type_info.second);

  // Cast to common AST nodes and return with the types.
  const auto cast_branches_type_info = branches_type_info
    | genex::views::transform([](auto &&x) {
      return MakePair(x.first->template ToUnchecked<Ast>(), x.second);
    })
    | genex::to<Vec>();
  return {cast_master_branch_type_info, cast_branches_type_info};
}

auto spp::analyse::utils::case_utils::ValidateInconsistentMemory(
  Ast *parent, Vec<CaseExpressionBranchAst*> const &branches,
  CaseCoverage const &coverage, VariableSymbol *subject,
  SubjectEdges const *subject_edges, ScopeManager *sm,
  CompilerMetaData *meta) -> void {
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

  // The state before any branch has run. Each branch starts
  // from it, and it stands in as the path that falls through
  // every branch, where there is one. Each branch's state
  // is kept to be joined.
  const auto pre_analysis = memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope);
  auto paths = Vec<Pair<CaseExpressionBranchAst*, memory_state::ScopeSnapshot>>();
  for (auto const &[i, branch] : branches | genex::views::enumerate) {
    // The branch applies its match edge to the subject.
    meta->CaseSubjectOnMatch = subject_edges != nullptr
      ? subject_edges->OnMatch[i]
      : meta::SubjectOnMatch::kKeep;
    branch->Stage8_CheckMemory(sm, meta);
    paths.emplace_back(branch, memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope));
    memory_state::RestoreSnapshot(pre_analysis);
  }

  // On the fall-through path, the subject holds whichever
  // members no branch matched.
  const auto falls_through = coverage.FallsThrough;
  paths.emplace_back(nullptr, pre_analysis);
  const auto discharged = subject_edges != nullptr
    and subject_edges->OnFallThrough == meta::SubjectOnMatch::kDischarge;

  // A branch that never finishes leaves no state behind to
  // agree with. Worked out once per branch, not per symbol:
  // it infers the body's type.
  auto diverging = Set<CaseExpressionBranchAst const*>();
  for (auto branch : branches) {
    if (expr_utils::Diverges(*branch->Body, sm, meta)) { diverging.insert(branch); }
  }

  // A branch diverges if it in the set of diverging case
  // expression asts.
  const auto diverges = [&](CaseExpressionBranchAst const *branch) {
    return diverging.contains(branch);
  };

  // A branch after every value was matched, is never taken.
  const auto reachable = [&](CaseExpressionBranchAst const *branch) {
    const auto at = genex::position(branches, genex::operations::eq_fixed{branch});
    return not at.has_value() or coverage.Reachable[*at];
  };

  // The first branch that finishes is the path carried on.
  const auto non_terminating_branch = genex::find_if(
    branches, [&](auto const &x) { return not diverges(x); });

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
  const auto first_branch = non_terminating_branch == branches.end() ? case_named : path_of(*non_terminating_branch);
  auto const &first_path = non_terminating_branch != branches.end()
    ? paths.at(static_cast<std::size_t>(genex::iterators::distance(branches.begin(), non_terminating_branch))).second
    : paths.back().second;

  const auto has_else_branch = not branches.empty()
    ? branches.back()->Patterns[0]->To<CasePatternVariantElseAst>()
    : nullptr;
  const auto skip_else = has_else_branch and has_else_branch->MarkedForIterLoopExit();

  // The state after the "case" is every path's state out of
  // it, joined: the branches that finish, and without an
  // "else" (when no branch may run at all) the state the
  // "case" was entered with (the null entry).
  const auto leaves = [&](CaseExpressionBranchAst const *branch) {
    return not((branch == nullptr and not falls_through) or (branch != nullptr and diverges(branch))
      or (branch != nullptr and not reachable(branch))
      or (skip_else and not branches.empty() and branch == branches.back()));
  };
  const auto state_on = [&](memory_state::ScopeSnapshot const &path, VariableSymbol const &sym) {
    auto state = memory_state::StateIn(path, sym);
    if (discharged and &sym == subject and &path == &paths.back().second) { state.OwesNothing = true; }
    return memory_state::SettleFullyTaken(state, sym, *sm->CurrentScope);
  };
  for (auto sym : var_syms) {
    auto joined = state_on(first_path, *sym);
    for (auto const &[branch_or_null, path] : paths) {
      if (leaves(branch_or_null)) {
        memory_state::Join(joined, state_on(path, *sym), first_branch, path_of(branch_or_null));
      }
    }
    memory_state::StateOf(*sm->CurrentScope, *sym) = joined;
  }

  // The loans held after the "case", likewise.
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  const auto loans_on = [](memory_state::ScopeSnapshot const &path) { return path.Loans; };
  loans = loans_on(first_path);
  for (auto const &[branch_or_null, path] : paths) {
    if (leaves(branch_or_null)) {
      memory_state::JoinLoans(loans, loans_on(path), first_branch, path_of(branch_or_null));
    }
  }
}

auto spp::analyse::utils::case_utils::ApplySubjectOnMatch(
  CaseExpressionBranchAst const &branch,
  ScopeManager &sm,
  CompilerMetaData const &meta)
  -> void {
  // "case x of" takes the symbol itself; "case x.inner of"
  // takes one region of it, a partial move like any other.
  // Only a whole subject can be found to owe nothing: the
  // fact is held per symbol.
  const auto subject = meta.CaseSubject;
  auto const *const cond = meta.CaseCondition;
  if (subject == nullptr or cond == nullptr) { return; }
  const auto whole = cond->To<IdentifierAst>() != nullptr;

  // The patterns took it apart, so a part they left unbound
  // would be left with no owner once it is taken.
  if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kTake) {
    const auto taken = regions::PlaceOf(*cond, *sm.CurrentScope);
    if (const auto skipped = taken.has_value() ? regions::FirstUnaccountedPart(*subject, *taken, sm) : Str();
      not skipped.empty()) {
      auto const *const blamed = branch.Patterns.empty()
        ? static_cast<Ast const*>(&branch)
        : static_cast<Ast const*>(branch.Patterns[0].get());
      Raise<errors::SppDestructureSkipsOwnedPartError>(ERR_ARGS(*blamed, *subject->Name, StrView(skipped)));
    }
  }

  // If we are moving an identifier part, then mark the state
  // as "moved by", so it cannot be used until initialized
  // again.
  if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kTake and whole) {
    memory_state::StateOf(*sm.CurrentScope, *subject).MovedBy(*cond);
  }

  // If we are moving a non-identifier part, then this is a
  // partial move, so get its place, and mark the partial
  // move.
  else if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kTake) {
    memory_state::StateOf(*sm.CurrentScope, *subject).PartMovedBy(regions::PlaceAtOf(*cond, *sm.CurrentScope));
  }

  // If the discharge flag has been set and it is a identifier
  // ast, then mark the "owes nothing" as true.
  else if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kDischarge and whole) {
    memory_state::StateOf(*sm.CurrentScope, *subject).OwesNothing = true;
  }
}
