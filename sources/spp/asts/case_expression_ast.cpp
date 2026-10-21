module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.pattern_solver;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.is_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import std;

SPP_MOD_BEGIN
CaseExpressionAst::CaseExpressionAst(
  decltype(TokCase) &&tok_case,
  decltype(Cond) &&cond,
  decltype(TokOf) &&tok_of,
  decltype(Branches) &&branches) :
  TokCase(std::move(tok_case)),
  Cond(std::move(cond)),
  TokOf(std::move(tok_of)),
  Branches(std::move(branches)) {
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokCase, SppTokenType::KW_CASE, "case", cond ? cond->PosStart() : 0);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokOf, SppTokenType::KW_OF, "of", cond ? cond->PosEnd() : 0);
}

CaseExpressionAst::~CaseExpressionAst() = default;

auto CaseExpressionAst::NewNonPatternMatch(
  decltype(TokCase) &&tok_case, decltype(Cond) &&cond, Unique<InnerScopeExpressionAst> &&first,
  decltype(Branches) &&branches) -> Unique<CaseExpressionAst> {
  // A condition that is one "is" ("case x is P { .. }") is the
  // pattern matching "case x of { is P { .. } }", so the pattern
  // is matched against the subject directly, and its coverage
  // seen, rather than through a boolean.
  if (const auto is_expr = cond->To<IsExpressionAst>()) {
    auto is_patterns = Vec<Unique<CasePatternVariantAst>>();
    is_patterns.emplace_back(std::move(is_expr->Rhs));
    branches.insert(
      branches.begin(),
      MakeUnique<CaseExpressionBranchAst>(
        std::move(is_expr->TokOp), std::move(is_patterns), nullptr, std::move(first)));
    return MakeUnique<CaseExpressionAst>(
      std::move(tok_case), std::move(is_expr->Lhs), nullptr, std::move(branches));
  }

  // Convert consecutive if/else-if/else branches into case
  // pattern matching.
  auto patterns = Vec<Unique<CasePatternVariantAst>>(1);
  patterns[0] = MakeUnique<CasePatternVariantExpressionAst>(
    BooleanLiteralAst::True(tok_case->PosStart()));
  auto first_branch = MakeUnique<CaseExpressionBranchAst>(
    nullptr, std::move(patterns), nullptr, std::move(first));
  branches.insert(branches.begin(), std::move(first_branch));

  // Return the final, newly created, case expression AST.
  auto out = MakeUnique<CaseExpressionAst>(
    std::move(tok_case), std::move(cond), nullptr, std::move(branches));
  return out;
}

auto CaseExpressionAst::PosStart() const -> std::size_t {
  // Use the "case" token.
  return TokCase->PosStart();
}

auto CaseExpressionAst::PosEnd() const -> std::size_t {
  // Use the "of" token, or condition.
  return TokOf != nullptr ? TokOf->PosEnd() : Cond->PosEnd();
}

auto CaseExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<CaseExpressionAst>(
    AstClone(TokCase),
    AstClone(Cond),
    AstClone(TokOf),
    AstCloneVec(Branches));
  c->LoweredFromIsExpr = LoweredFromIsExpr;
  c->LoweredFromTryOperator = LoweredFromTryOperator;
  return c;
}

auto CaseExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokCase).append(" ");
  SPP_STRING_APPEND(Cond).append(" ");
  SPP_STRING_APPEND(TokOf).append(" ");
  SPP_STRING_EXTEND(Branches, "\n");
  SPP_STRING_END;
}

auto CaseExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Create the scope for the case expression.
  using namespace spp::analyse::errors;
  using namespace spp::analyse::utils;
  auto scope_name = ScopeBlockName::FromParts(
    "case-expr", {}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), nullptr);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  const auto cond_bindings_before = meta->AddedIsBindings.size();

  // The condition is never what the "case" is assigned to, so it
  // does not see that target: a lowered "is" as the condition has
  // "Bool" branches of its own to agree on.
  SPP_DEREF_ALLOW_MOVE_HELPER(Cond) {
    const auto _meta_guard = MetaGuard(meta);
    meta->AllowMoveDeref = true;
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    Cond->Stage7_AnalyseSemantics(sm, meta);
  }
  else {
    const auto _meta_guard = MetaGuard(meta);
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    Cond->Stage7_AnalyseSemantics(sm, meta);
  }

  // Analyse the condition expression.
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Cond, *sm),
    ERR_ARGS(*Cond));

  // The subject as the branches' tests read it, so that they
  // never evaluate the condition again.
  _SubjectType = Cond->InferType(sm, meta);
  _SubjectValue = pattern_solver::DeclareSubjectValue(
    TypeRef::Of(*_SubjectType, *sm->CurrentScope),
    Cond->PosStart(), *sm);

  // Without "of", the condition is what the first branch
  // tests directly (against "true", with no operator), so it
  // has to be a boolean. "TokOf" is filled in either way, so
  // the form is told apart by that first branch.
  const auto tests_condition_directly = not Branches.empty()
    and Branches[0]->Op == nullptr
    and not Branches[0]->Patterns.empty()
    and Branches[0]->Patterns[0]->To<CasePatternVariantExpressionAst>() != nullptr;

  if (tests_condition_directly and not LoweredFromIsExpr
    and not type_predicates::IsTypeBool(Cond->InferTypeRef(sm, meta), *sm->CurrentScope)) {
    const auto cond_ty = Cond->InferType(sm, meta);
    Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Cond, *cond_ty, "case"));
  }

  // What an "is" in the condition binds only holds in the branch
  // taken when it matched - the first, for the form without "of".
  auto cond_bindings = Vec<VariableSymbol*>();
  if (tests_condition_directly) {
    for (auto i = cond_bindings_before; i < meta->AddedIsBindings.size(); ++i) {
      cond_bindings.emplace_back(meta->AddedIsBindings[i]);
    }
  }

  // Analyse each branch of the case expression.
  for (auto const &branch : Branches) {
    const auto expired_before = meta->ExpiredIsBindings.size();
    if (branch != Branches[0]) { meta->ExpiredIsBindings.append_range(cond_bindings); }
    // Check the "else" branch is the last branch (also checks
    // there is only 1 "else" branch).
    RaiseIf<SppCaseBranchElseNotLastError>(
      branch->Patterns[0]->To<CasePatternVariantElseAst>() and branch != Branches.back(),
      ERR_ARGS(*branch, *Branches.back()));

    // Analyse the branch.
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->CaseCondition = Cond.get();
      branch->Stage7_AnalyseSemantics(sm, meta);
    }
    meta->ExpiredIsBindings.resize(expired_before);
  }

  // Enforce consistent branch type return values; either the
  // values are being propagated up to an identifier, or they
  // should all be void.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->CaseCondition = Cond.get();
    meta->IgnoreMissingElseBranchForInference = true;
    case_utils::ValidateInconsistentTypes(
      Branches | genex::views::ptr | genex::to<Vec>(), *sm, meta);
  }

  // What the branches match, worked out fresh for each analysis.
  // A branch that can never match (after an "else", after every
  // value is matched) is an error: it has no use. One the
  // compiler wrote itself (the "else" of a lowered "is") is not
  // the author's to remove.
  _Plan = nullptr;
  auto const &coverage = Plan(sm, meta).Coverage;
  for (auto const &[i, branch] : Branches | genex::views::ptr | genex::views::enumerate) {
    if (coverage.Reachable[i] or branch->PosStart() == 0) { continue; }
    Raise<SppCaseBranchUnreachableError>(ERR_ARGS(*branch, *Branches[coverage.ShadowedBy[i].value_or(i)]));
  }

  // Move out of the case expression scope.
  sm->MoveOutOfCurrentScope();
}

auto CaseExpressionAst::Plan(
  ScopeManager *sm, CompilerMetaData *meta) -> MatchPlan const& {
  // Use the cached plan that has already been computed, if it
  // exists.
  IMPORT_UTILS;
  if (_Plan != nullptr) { return *_Plan; }

  // The subject type is the condition's type, as it was before
  // any branch narrowed it. This isn't always boolean, for
  // example if we are doing a pattern matching operation: array,
  // tuple, class etc.
  const auto subject_type = _SubjectType != nullptr
    ? _SubjectType
    : Cond->InferType(sm, meta);

  const auto subject = TypeRef::Of(*subject_type, *sm->CurrentScope);

  // Solve the patterns once, against the subject's type, and
  // hand each branch its part of the plan.
  auto solver = pattern_solver::PatternSolver(
    Branches | genex::views::ptr | genex::to<Vec>(), subject, _SubjectValue, *sm->CurrentScope);
  _Plan = MakeShared<pattern_solver::MatchPlan>(solver.Solve(*sm, *meta));
  for (auto const &[i, branch] : Branches | genex::views::ptr | genex::views::enumerate) {
    branch->UsePlan(_Plan.get(), i);
  }
  return *_Plan;
}

auto CaseExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Move into the "case" scope and check the memory status
  // of the symbols in the branches.
  sm->MoveToNextScope();

  // Check the memory state of the condition.
  Cond->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(
    *Cond, *Cond, *sm, meta,
    {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false});

  // The subject, when it is a value this "case" can take
  // one it owns (not a borrow, which is someone else's to
  // give away) and that is not "Copy" (copying leaves the
  // original in place).
  const auto cond_ref = Cond->InferTypeRef(sm, meta);
  const auto cond_sym = cond_ref.Symbol != nullptr
    and not cond_ref.Symbol->IsCopyable()
    and not cond_ref.IsBorrowed()
    ? sm->CurrentScope->FindVarSymbolOutermost(*Cond).first
    : nullptr;

  const auto subject = cond_sym != nullptr
    and cond_sym->MemInfo->AstBorrowed == nullptr
    ? cond_sym
    : nullptr;

  const auto branches = Branches | genex::views::ptr | genex::to<Vec>();
  auto const &plan = Plan(sm, meta);
  auto const &coverage = plan.Coverage;
  const auto temporary = subject == nullptr and linear_utils::IsLinearTemporary(*Cond, *sm, meta);

  // Handle unconsumed temporaries. For example, when a linear
  // temporary is the condition "case make_str() of {", then
  // one branch must take ownership of it. Unless its copyable.
  const auto flow = memory_state::FlowOf(*sm->CurrentScope);
  const auto kept_before = flow != nullptr ? flow->KeptTemporaries.size() : 0uz;
  if (temporary) {
    for (auto const &[i, branch] : branches | genex::views::enumerate) {
      const auto keeps = coverage.Reachable[i] and plan.Edges.OnMatch[i] == SubjectOnMatch::kKeep;
      if (not keeps) { continue; }

      // A reachable branch that keeps the case condition temporary
      // (does not take it) raises a linear error, unless the body
      // of the branch diverges.
      if (not expr_utils::Diverges(*branch->Body, sm, meta)) {
        linear_utils::RaiseTemporaryNotConsumed(
          *Cond, *branch, "This branch keeps it, so nothing owns it after the \"case\"", *sm, meta);
      }

      // Log the temporary into the flow, against the branch that
      // maintains it.
      if (flow != nullptr) {
        flow->KeptTemporaries.emplace_back(Cond.get(), branch);
      }
    }

    // If the temporary falls through to the end, the same error
    // is reported, against the case rather than the branch.
    if (coverage.FallsThrough and plan.Edges.OnFallThrough == SubjectOnMatch::kKeep) {
      linear_utils::RaiseTemporaryNotConsumed(
        *Cond, *this, "Falling through keeps it, so nothing owns it after the \"case\"", *sm, meta);
    }
  }

  // Validate the memory state across all branches (also calls
  // stage 8 from within).
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->CaseCondition = Cond.get();
    meta->CaseSubject = subject;
    case_utils::ValidateInconsistentMemory(
      this, branches, coverage, subject, subject != nullptr ? &plan.Edges : nullptr, sm, meta);
  }
  if (flow != nullptr) { flow->KeptTemporaries.resize(kept_before); }

  // Move out of the case expression scope.
  sm->MoveOutOfCurrentScope();
}

auto CaseExpressionAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Scopes.
  sm->MoveToNextScope();

  // Load meta information for compile-time resolution.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->CaseCondition = Cond.get();

    // The subject's comptime value, for the tests to read.
    IMPORT_UTILS;
    auto const &plan = Plan(sm, meta);
    if (plan.ReadsSubject) {
      Cond->Stage9_CompTimeResolve(sm, meta);
      if (meta->CompTimeResult != nullptr) { case_utils::BindSubjectCompTimeValue(plan, *meta->CompTimeResult); }
    }

    // Delegate to the branches' compile-time resolution (break
    // at first match).
    meta->CompTimeResult = nullptr;
    for (auto const &branch : Branches) {
      branch->Stage9_CompTimeResolve(sm, meta);
      if (meta->CompTimeResult != nullptr) { break; }
    }

    // Otherwise, if no branches matched, this is non-returning,
    // so nullptr is fine.
  }
  sm->MoveOutOfCurrentScope();
}

auto CaseExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  // Scope shift.
  sm->MoveToNextScope();

  // Determine if this "case" will be yielding an expression,
  // and generate the condition. The expression flag is needed
  // when considering PHI node handling.
  const auto uid = "." + Uid();

  // The condition first: an "is" in it declares the names it binds (and the subject it narrows), which the branches
  // read, and so does inferring what this "case" yields.
  const auto llvm_cond = [&] {
    const auto _meta_guard = MetaGuard(meta);
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    meta->LlvmAssignmentTarget = nullptr;
    return Cond->Stage11_CodeGen(sm, meta, ctx);
  }();

  // The subject as the branches' tests read it.
  if (auto const &plan = Plan(sm, meta); plan.ReadsSubject) {
    case_utils::BindSubjectValue(plan, llvm_cond, ctx);
  }

  // A "case" yields a value when something is catching it, or
  // when it is the desugaring of an "is", which is a boolean
  // expression wherever it appears, including the condition
  // positions that assign nothing, or being passed straight to
  // a call ("f(case b { x } else { y })")
  const auto falls_through = Plan(sm, meta).Coverage.FallsThrough;
  const auto yields_value = not falls_through and not LoweredFromIsExpr and [&] {
    const auto _meta_guard = MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;

    // A target type with no target is one an enclosing loop or
    // "case" left for its own values ("exit x", a branch's final
    // expression), not one this "case" is being assigned to. It
    // would be checked against this case's branches as if it were.
    if (meta->AssignmentTarget == nullptr) { meta->AssignmentTargetType = nullptr; }
    return not codegen::IsValuelessType(
      codegen::GetLlvmTypeOf(InferTypeRef(sm, meta), ctx));
  }();

  const auto is_expr = meta->AssignmentTarget != nullptr
    or LoweredFromIsExpr
    or LoweredFromTryOperator
    or yields_value;

  // Get the function, and create the end basic block. We
  // define "entry" and "end" zones for the "case" expression,
  // and start in the "entry" zone.
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto case_entry_bb = llvm::BasicBlock::Create(
    *ctx->Context, "case.entry" + uid, func);
  const auto case_end_bb = llvm::BasicBlock::Create(
    *ctx->Context, "case.end" + uid, func);
  ctx->Builder.CreateBr(case_entry_bb);

  auto phi = static_cast<llvm::PHINode*>(nullptr);
  auto ret_type = Shared<TypeAst>(nullptr);
  if (is_expr) {
    // The phi merges the value yielded by each branch, so it
    // belongs in the end block (where every branch body branches
    // to at the end of the branch's body), not the entry block.
    ctx->Builder.SetInsertPoint(case_end_bb);

    // Create a PHI handler with "n" reserved values, 1 for each
    // branch that might get entered for this "case" expression.
    const auto n = static_cast<unsigned>(Branches.size());

    // An "is" desugars into branches yielding "true" and "false",
    // so the merged value is a boolean by construction. Inferring
    // it instead would re-walk the branches with the scope
    // manager standing where codegen left it rather than where
    // analysis did, and a disagreement there surfaces as a type
    // mismatch raised from the middle of code generation.
    // "ret_type" stays null with it, which is what stops the
    // branches trying to widen a bool into a variant on the way
    // into the phi.
    const auto llvm_phi_ty = [&] {
      if (LoweredFromIsExpr) { return static_cast<llvm::Type*>(llvm::Type::getInt1Ty(*ctx->Context)); }
      ret_type = InferType(sm, meta);
      return codegen::GetLlvmTypeOf(TypeRef::Of(*ret_type, *sm->CurrentScope), ctx);
    }();

    if (not codegen::IsValuelessType(llvm_phi_ty)) {
      phi = ctx->Builder.CreatePHI(
        llvm_phi_ty, n, "case.phi" + uid);
    }
  }

  // Set "case" information to the meta struct for branches and
  // patterns to use.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->CaseCondition = Cond.get();
    meta->LlvmCaseCondition = llvm_cond;
    meta->LlvmEndBB = case_end_bb;
    meta->LlvmPhi = phi;

    // Branches need the yielded type, not just the phi's llvm type,
    // so they can wrap a member value into a variant.
    meta->AssignmentTargetType = ret_type;

    // Generate each branch (no return value because branches don't
    // return generated ir, rather the PHI nodes get registered as
    // ret values, and the body code is generated by branching (auto
    // bound).
    ctx->Builder.SetInsertPoint(case_entry_bb);
    for (auto const &branch : Branches) {
      branch->Stage11_CodeGen(sm, meta, ctx);
    }

    // After the last branch, the insert point is the fall-through
    // block reached when no pattern matched (the end block), so it
    // needs a terminator. Semantic analysis guarantees that for expr
    // assignments, one branch will be taken (every value is matched),
    // and a "case" matching every value never reaches it either.
    if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      if (is_expr or not falls_through) { ctx->Builder.CreateUnreachable(); }
      else { ctx->Builder.CreateBr(case_end_bb); }
    }

    // Clean up by setting the new insertion point as the end of the
    // case block (for new code to generate at following this "case"
    // expr), and exit the scope.
  }
  ctx->Builder.SetInsertPoint(case_end_bb);
  sm->MoveOutOfCurrentScope();
  return phi;
}

auto CaseExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  IMPORT_UTILS;
  using generate::common_types_precompiled::VoidAt;

  // Ensure consistency across branches. Also done in "Stage7_AnalyseSemantics", which is what covers a case in
  // statement position - nothing asks one of those for its type, so this would never run for it.
  //
  // Todo: the other half of the rule is not enforced. Branches must agree with each other, and where the case is not
  //  used as an expression that agreed type should be "Void" - "case x of { == 1 { 1 } else { 2 } }" discards an
  //  "S32" that nothing asked for. A case carries no signal for which of the two positions it is in, which is what
  //  the missing half needs; see the red test in "test_ast_case_expression.cpp".
  auto [master_branch_type_info, branches_type_info] = case_utils::ValidateInconsistentTypes(
    Branches | genex::views::ptr | genex::to<Vec>(), *sm, meta);

  // A "case" that can fall through every branch has no value on that path, so it needs an "else" to be one: either
  // written, or every value matched by the branches ("pattern_solver::PatternSolver").
  // Before Stage 7 has worked out what it covers (the patterns are not analysed yet), only an "else" says it does not.
  const auto falls_through = _Plan != nullptr
    ? _Plan->Coverage.FallsThrough
    : not pattern_solver::IsElseBranch(*Branches.back());
  RaiseIf<SppCaseBranchMissingElseError>(
    falls_through and not meta->IgnoreMissingElseBranchForInference,
    ERR_ARGS(*this, *Branches.back()));

  // A "case" with no "else" can finish without running any branch at all, so whatever its branches are, it is not
  // "Never": the fall-through path is reachable, and the value it produces on that path is no value. Handing back the
  // branches' type here instead is what made "case a { case b { abort() } }" crash - the inner case reads as "Never",
  // so the outer branch believes its body cannot complete and terminates the block the inner case falls through to
  // with "unreachable", which is exactly the path taken whenever "b" is false.
  //
  // @n
  // This is also half of the Todo above: a case that is not an expression yields nothing, and falling through is the
  // one case of that which can be told apart here, because covering every value is mandatory in expression position.
  if (falls_through) { return VoidAt(PosStart()); }

  // Nothing falls through, and every branch that can match diverges: no path leaves the "case" at all, so it is "!",
  // whatever its "ret" branches' own types. An "else { unreachable() }" used to supply the "!" this needs.
  if (_Plan != nullptr) {
    auto diverges = true;
    for (auto const &[i, branch] : Branches | genex::views::ptr | genex::views::enumerate) {
      if (_Plan->Coverage.Reachable[i] and not expr_utils::Diverges(*branch->Body, sm, meta)) {
        diverges = false;
      }
    }
    if (diverges) { return generate::common_types_precompiled::NeverAt(PosStart()); }
  }

  // Return the branches' return type. If there are any
  // branches, otherwise Void.
  return branches_type_info.empty()
    ? VoidAt(PosStart())
    : master_branch_type_info.second;
}

auto CaseExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The branches' agreed type, checked as written ("case_utils::ValidateInconsistentTypes"); resolved where it is read.
  const auto type = InferType(sm, meta);
  return type != nullptr ? TypeRef::Of(*type, *sm->CurrentScope) : TypeRef();
}

auto CaseExpressionAst::Terminates() const -> bool {
  // Every branch that can match has to terminate, and some branch always has to run. A "case" that can fall through
  // every branch matches nothing on that path, so "case a { gen 1 ret }" ends the scope only when "a" holds - and the
  // statement after it is reachable. Before Stage 7 has worked out what it covers, only an "else" says it cannot.
  if (Branches.empty()) { return false; }
  if (_Plan == nullptr) {
    if (not analyse::utils::pattern_solver::IsElseBranch(*Branches.back())) { return false; }
    return not genex::any_of(Branches, [](auto const &branch) { return not branch->Body->Terminates(); });
  }
  if (_Plan->Coverage.FallsThrough) { return false; }
  for (auto const &[i, branch] : Branches | genex::views::ptr | genex::views::enumerate) {
    if (_Plan->Coverage.Reachable[i] and not branch->Body->Terminates()) { return false; }
  }
  return true;
}

SPP_MOD_END
