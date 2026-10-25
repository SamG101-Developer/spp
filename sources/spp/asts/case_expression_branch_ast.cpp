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
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
CaseExpressionBranchAst::CaseExpressionBranchAst(
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

CaseExpressionBranchAst::~CaseExpressionBranchAst() = default;

auto CaseExpressionBranchAst::PosStart() const -> std::size_t {
  // Use the op or first pattern
  return Op ? Op->PosStart() : Patterns.front()->PosStart();
}

auto CaseExpressionBranchAst::PosEnd() const -> std::size_t {
  // Use the final pattern.
  return Patterns.back()->PosEnd();
}

auto CaseExpressionBranchAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, carrying over the
  // desugaring marker.
  auto cloned = MakeUnique<CaseExpressionBranchAst>(
    AstClone(Op),
    AstCloneVec(Patterns),
    AstClone(Guard),
    AstClone(Body));
  return cloned;
}

auto CaseExpressionBranchAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Op).append(" ");
  SPP_STRING_EXTEND(Patterns, ", ");
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_APPEND(Guard).append(Guard ? " " : "");
  SPP_STRING_APPEND(Body);
  SPP_STRING_END;
}

auto CaseExpressionBranchAst::Stage7_AnalyseSemantics(
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
    for (auto const &[i, p] : Patterns | genex::views::ptr | genex::views::enumerate) {
      const auto pe = p->To<CasePatternVariantExpressionAst>();
      if (pe == nullptr) { continue; }

      auto subject = MakeUnique<IdentifierAst>(
        meta->CaseCondition->PosStart(), pattern_solver::SubjectValueName());
      _MappedPatFns[i] = MakeUnique<BinaryExpressionAst>(std::move(subject), AstClone(Op), AstClone(pe->Expr));
      _MappedPatFns[i]->Stage7_AnalyseSemantics(sm, meta);
    }
  }

  // Analyse the guard and body.
  if (Guard) { Guard->Stage7_AnalyseSemantics(sm, meta); }
  Body->Stage7_AnalyseSemantics(sm, meta);

  // Exit the scope.
  sm->MoveOutOfCurrentScope();
}

auto CaseExpressionBranchAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Move into the branch's scope.
  sm->MoveToNextScope();

  // Check the patterns and guard, then the match edge they
  // lead to (what the branch does with the "case" subject),
  // then the body.
  IMPORT_UTILS;
  for (auto const &p : Patterns) { p->Stage8_CheckMemory(sm, meta); }
  if (Guard) { Guard->Stage8_CheckMemory(sm, meta); }
  case_utils::ApplySubjectOnMatch(*this, *sm, *meta);
  Body->Stage8_CheckMemory(sm, meta);

  // The pattern's bindings end with the branch, and so do
  // the loans they hold.
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  for (const auto sym : sm->CurrentScope->GetAllVarSymbols(true)) {
    memory_state::ReleaseLoans(loans, *sym);
  }

  // Move out of the branch's scope.
  sm->MoveOutOfCurrentScope();
}

auto CaseExpressionBranchAst::Stage9_CompTimeResolve(
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
  for (auto const &[i, pattern] : Patterns | genex::views::ptr | genex::views::enumerate) {
    auto *tested = tests_condition_directly
      ? static_cast<Ast*>(meta->CaseCondition)
      : _MappedPatFns[i] != nullptr
      ? static_cast<Ast*>(_MappedPatFns[i].get())
      : static_cast<Ast*>(pattern);

    // Determine if this branch is not a match (false): the checks
    // the plan says the pattern makes, before the pattern binds
    // anything (a binding reads a payload only its tag says is
    // there), and then the pattern itself.
    const auto checks_hold = tested != pattern
      or case_utils::ResolvePatternChecks(
        *_CasePlan, _CasePlan->Branches[_CaseIndex].Checks[i], sm, meta);
    if (checks_hold) { tested->Stage9_CompTimeResolve(sm, meta); }
    const auto cmp_pat_bool = checks_hold and meta->CompTimeResult != nullptr
      ? meta->CompTimeResult->To<BooleanLiteralAst>()
      : nullptr;
    if (cmp_pat_bool == nullptr or not cmp_pat_bool->IsTrue()) {
      ignore = sm->SkipSubtree(sm->CurrentScope);
      continue;
    }

    // Check with the branch guard if it exists.
    if (Guard != nullptr) {
      Guard->Stage9_CompTimeResolve(sm, meta);
      const auto cmp_guard_bool = meta->CompTimeResult ? meta->CompTimeResult->To<BooleanLiteralAst>() : nullptr;
      if (not cmp_guard_bool or not cmp_guard_bool->IsTrue()) {
        ignore = sm->SkipSubtree(sm->CurrentScope);
        continue;
      }
    }

    // At this point, the correct branch has been identified, so resolve the body.
    Body->Stage9_CompTimeResolve(sm, meta);
    sm->MoveOutOfCurrentScope();
    return;
  }

  // None of the patterns on this branch matched.
  meta->CompTimeResult = nullptr;
  sm->MoveOutOfCurrentScope();
}

auto CaseExpressionBranchAst::Stage11_CodeGen(
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
    ctx->Builder.CreateCondBr(cond, guard_bb, next_bb);
    ctx->Builder.SetInsertPoint(guard_bb);
    const auto guard_cond = Guard->Stage11_CodeGen(sm, meta, ctx);
    ctx->Builder.CreateCondBr(guard_cond, body_bb, next_bb);
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
    if (meta->LlvmPhi != nullptr) { meta->LlvmPhi->addIncoming(llvm_val, incoming_bb); }
    ctx->Builder.CreateBr(meta->LlvmEndBB);
  }

  // Set the current point to the "next" zone, so that the next
  // branch can be added in the right place. Move out of the
  // branch's scope.
  ctx->Builder.SetInsertPoint(next_bb);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto CaseExpressionBranchAst::UsePlan(
  MatchPlan const *plan, const std::size_t index) -> void {
  _CasePlan = plan;
  _CaseIndex = index;
}

auto CaseExpressionBranchAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Forward type inference to the body.
  return Body->InferType(sm, meta);
}

auto CaseExpressionBranchAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return Body->InferTypeRef(sm, meta);
}

auto CaseExpressionBranchAst::_CodegenCombinePatterns(
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

SPP_MOD_END
