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
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import std;

SPP_MOD_BEGIN
spp::asts::CaseExpressionAst::CaseExpressionAst(
  decltype(TokCase) &&tok_case,
  decltype(Cond) &&cond,
  decltype(TokOf) &&tok_of,
  decltype(Branches) &&branches) :
  TokCase(std::move(tok_case)),
  Cond(std::move(cond)),
  TokOf(std::move(tok_of)),
  Branches(std::move(branches)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCase, lex::SppTokenType::KW_CASE, "case", cond ? cond->PosStart() : 0);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokOf, lex::SppTokenType::KW_OF, "of", cond ? cond->PosEnd() : 0);
}

spp::asts::CaseExpressionAst::~CaseExpressionAst() = default;

auto spp::asts::CaseExpressionAst::NewNonPatternMatch(
  decltype(TokCase) &&tok_case,
  decltype(Cond) &&cond,
  Unique<InnerScopeExpressionAst> &&first,
  decltype(Branches) &&branches) -> Unique<CaseExpressionAst> {
  // Convert consecutive if/else-if/else branches into case pattern matching.
  auto patterns = Vec<Unique<CasePatternVariantAst>>(1);
  patterns[0] = MakeUnique<CasePatternVariantExpressionAst>(BooleanLiteralAst::True(tok_case->PosStart()));
  auto first_branch = MakeUnique<CaseExpressionBranchAst>(nullptr, std::move(patterns), nullptr, std::move(first));
  branches.Insert(branches.begin(), std::move(first_branch));

  // Return the final, newly created, case expression AST.
  auto out = MakeUnique<CaseExpressionAst>(std::move(tok_case), std::move(cond), nullptr, std::move(branches));
  return out;
}

auto spp::asts::CaseExpressionAst::PosStart() const
  -> std::size_t {
  // Use the "case" token.
  return TokCase->PosStart();
}

auto spp::asts::CaseExpressionAst::PosEnd() const
  -> std::size_t {
  // Use the "of" token, or condition.
  return TokOf != nullptr ? TokOf->PosEnd() : Cond->PosEnd();
}

auto spp::asts::CaseExpressionAst::Clone() const
  -> Unique<Ast> {
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

auto spp::asts::CaseExpressionAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokCase).append(" ");
  SPP_STRING_APPEND(Cond).append(" ");
  SPP_STRING_APPEND(TokOf).append(" ");
  SPP_STRING_EXTEND(Branches, "\n");
  SPP_STRING_END;
}

auto spp::asts::CaseExpressionAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Alias the common utils functions and types.
  using analyse::errors::SppCaseBranchElseNotLastError;
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

  // Create the scope for the case expression.
  auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
    "case-expr", {}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), nullptr);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  SPP_DEREF_ALLOW_MOVE_HELPER(Cond) {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->AllowMoveDeref = true;
    Cond->Stage7_AnalyseSemantics(sm, meta);
  }
  else {
    Cond->Stage7_AnalyseSemantics(sm, meta);
  }

  // Analyse the condition expression.
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(*Cond, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Cond));

  // Every branch is analysed from the memory state the case
  // was entered with. A branch is one alternative, not a
  // continuation of the one before it, so initializing an
  // immutable "let" in one branch must not read as a second
  // initialization in the next.
  const auto pre_branch_state = sm->CurrentScope->AllVarSymbols()
    | genex::views::transform([](auto *x) { return MakePair(x, x->MemInfo->Snapshot()); })
    | genex::to<Vec>();
  auto post_first_branch_state = decltype(pre_branch_state)();

  // Analyse eac branch of the case expression.
  for (auto const &branch : Branches) {
    // Check the "else" branch is the last branch (also checks
    // there is only 1 "else" branch).
    RaiseIf<SppCaseBranchElseNotLastError>(
      branch->Patterns[0]->To<CasePatternVariantElseAst>() and branch != Branches.Back(),
      {sm->CurrentScope}, ERR_ARGS(*branch, *Branches.Back()));

    // Analyse the branch.
    for (auto const &[sym, snapshot] : pre_branch_state) {
      sym->MemInfo->FillFromSnapshot(snapshot);
    }

    {
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->CaseCondition = Cond.get();
      branch->Stage7_AnalyseSemantics(sm, meta);
    }

    // Keep the first branch's resulting state as the one the
    // code after the case continues from, matching how stage 8
    // resolves the post-case state.
    if (post_first_branch_state.IsEmpty()) {
      post_first_branch_state = pre_branch_state
        | genex::views::transform([](auto const &x) { return MakePair(x.first, x.first->MemInfo->Snapshot()); })
        | genex::to<Vec>();
    }
  }

  for (auto const &[sym, snapshot] : post_first_branch_state) {
    sym->MemInfo->FillFromSnapshot(snapshot);
  }

  // Enforce consistent branch type return values; either the
  // values are being roppropagated up to an identifier, or
  // they should all be void.
  {
    using analyse::utils::case_utils::ValidateInconsistentTypes;
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->CaseCondition = Cond.get();
    meta->IgnoreMissingElseBranchForInference = true;
    ValidateInconsistentTypes(Branches | genex::views::ptr | genex::to<Vec>(), *sm, meta);
  }

  // Move out of the case expression scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Alias the common utils functions and types.
  using analyse::utils::case_utils::ValidateInconsistentMemory;
  using analyse::utils::mem_utils::ValidateSymbolMemory;

  // Move into the "case" scope and check the memory status of the symbols in the branches.
  sm->MoveToNextScope();

  // Check the memory state of the condition.
  Cond->Stage8_CheckMemory(sm, meta);
  ValidateSymbolMemory(*Cond, *Cond, *sm, true, true, false, false, meta);

  // Whether this "case" takes its subject is decided before the branches run, because a "ret" or a loop jump inside
  // one of them is checked while they run - and by then the subject has already been given up, even though the mark
  // itself cannot be made until the branches have bound off it.
  //
  // A borrowed subject is not this scope's to give away, and copying leaves the original in place, so a copyable one
  // is never taken however its patterns bind. That second guard is the one "ValidateSymbolMemory" applies through
  // "moves_value", repeated here because marking the move directly is what skips it.
  const auto binds_by_move = TokOf != nullptr and not LoweredFromIsExpr and genex::any_of(
    Branches, [](auto const &branch) {
      return genex::any_of(branch->Patterns, [](auto const &p) { return p->BindsByMove(); });
    });

  const auto cond_type = binds_by_move ? Cond->InferType(sm, meta) : nullptr;
  const auto cond_ty_sym = cond_type != nullptr ? sm->CurrentScope->GetTypeSymbol(cond_type.get()) : nullptr;
  const auto cond_sym = cond_ty_sym != nullptr and not cond_ty_sym->IsCopyable()
    ? sm->CurrentScope->GetVarSymbolOutermost(*Cond).first
    : nullptr;

  const auto takes_subject = cond_sym != nullptr
    and cond_type->GetConvention() == nullptr
    and spp::get<0>(cond_sym->MemInfo->AstBorrowed) == nullptr;

  // Validate the memory state across all branches (also calls stage 8 from within).
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->CaseCondition = Cond.get();
    if (takes_subject) { meta->CaseConsumedSubjects.EmplaceBack(cond_sym->Name); }
    ValidateInconsistentMemory(
      this, Branches | genex::views::ptr | genex::to<Vec>(), takes_subject ? cond_sym : nullptr, sm, meta);
  }

  // The mark is made here, after the branches have bound off the subject and outside the per-branch snapshots
  // "ValidateInconsistentMemory" takes. That is what keeps the branches agreeing: every one of them, "else" included,
  // leaves the subject in the same state, because none of them is what moved it.
  //
  // Marked directly rather than through "ValidateSymbolMemory", whose inconsistency checks run whatever flags it is
  // given. Those checks are exactly what must not fire here: the branches having moved different parts of the subject
  // stops meaning anything once the whole of it is taken, because all of it is gone either way.
  if (takes_subject) {
    // "case x of" takes the symbol itself; "case x.inner of" takes one region of it, which is a partial move like any
    // other.
    if (Cond->To<IdentifierAst>() != nullptr) {
      cond_sym->MemInfo->MovedBy(*Cond, sm->CurrentScope);
      cond_sym->MemInfo->AstPartialMoves.Clear();
    }
    else {
      cond_sym->MemInfo->AstPartialMoves.EmplaceBack(Cond.get());
    }
    cond_sym->MemInfo->IsInconsistentlyMoved = std::nullopt;
    cond_sym->MemInfo->IsInconsistentlyPartiallyMoved = std::nullopt;
  }

  // Move out of the case expression scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionAst::Stage9_CompTimeResolve(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Scopes.
  sm->MoveToNextScope();

  // Load meta information for compile-time resolution.
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->CaseCondition = Cond.get();

    // Delegate to the branches' compile-time resolution (break at first match).
    meta->CmpResult = nullptr;
    for (auto const &branch : Branches) {
      branch->Stage9_CompTimeResolve(sm, meta);
      if (meta->CmpResult != nullptr) { break; }
    }

    // Otherwise, if no branches matched, this is non-returning, so nullptr is fine.
  }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Scope shift.
  sm->MoveToNextScope();

  // Determine if this "case" will be yielding an expression,
  // and generate the condition. The expression flag is needed
  // when considering PHI node handling.
  const auto uid = "." + spp::utils::Uid(this);

  // A "case" yields a value when something is catching it, or
  // when it is the desugaring of an "is", which is a boolean
  // expression wherever it appears, including the condition
  // positions that assign nothing.
  const auto is_expr = meta->AssignmentTarget != nullptr
    or LoweredFromIsExpr
    or LoweredFromTryOperator;
  const auto llvm_cond = Cond->Stage11_CodeGen(sm, meta, ctx);

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
    const auto n = static_cast<unsigned>(Branches.Len());

    // An "is" desugars into branches yielding "true" and "false", so the merged value is a boolean by construction.
    // Inferring it instead would re-walk the branches with the scope manager standing where codegen left it rather
    // than where analysis did, and a disagreement there surfaces as a type mismatch raised from the middle of code
    // generation. "ret_type" stays null with it, which is what stops the branches trying to widen a bool into a
    // variant on the way into the phi.
    const auto llvm_phi_ty = [&] {
      if (LoweredFromIsExpr) { return static_cast<llvm::Type*>(llvm::Type::getInt1Ty(*ctx->Context)); }
      ret_type = InferType(sm, meta);
      return codegen::GetLlvmTypeOf(*ret_type, *sm->CurrentScope, ctx);
    }();

    if (not codegen::IsValuelessType(llvm_phi_ty)) {
      phi = ctx->Builder.CreatePHI(
        llvm_phi_ty, n, "case.phi" + uid);
    }
  }

  // Set "case" information to the meta struct for branches and
  // patterns to use.
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->CaseCondition = Cond.get();
    meta->LlvmCaseCondition = llvm_cond;
    meta->LlvmEndBB = case_end_bb;
    meta->LlvmPhi = phi;

    // Branches need the yielded type, not just the phi's llvm type,
    // so they can wrap a member value into a variant.
    meta->AssignmentTargetType = ret_type;

    // Generate each branch (no return value because branches don't
    // return generated ir, rather the PHI nodes get registered as ret
    // values, and the body code is generated by branching (auto bound).
    ctx->Builder.SetInsertPoint(case_entry_bb);
    for (auto const &branch : Branches) {
      branch->Stage11_CodeGen(sm, meta, ctx);
    }

    // After the last branch, the insert point is the fall-through
    // block reached when no pattern matched (the end block), so it
    // needs a terminator. Semantic analysis guarantees that for expr
    // assignments, one branch will be taken ("else" pattern is used).
    if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      if (is_expr) { ctx->Builder.CreateUnreachable(); }
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

auto spp::asts::CaseExpressionAst::InferType(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Alias the common utils functions and types.
  using analyse::errors::SppCaseBranchMissingElseError;
  using analyse::utils::case_utils::ValidateInconsistentTypes;
  using generate::common_types::VoidType;

  // Ensure consistency across branches. Also done in "Stage7_AnalyseSemantics", which is what covers a case in
  // statement position - nothing asks one of those for its type, so this would never run for it.
  //
  // Todo: the other half of the rule is not enforced. Branches must agree with each other, and where the case is not
  //  used as an expression that agreed type should be "Void" - "case x of { == 1 { 1 } else { 2 } }" discards an
  //  "S32" that nothing asked for. A case carries no signal for which of the two positions it is in, which is what
  //  the missing half needs; see the red test in "test_ast_case_expression.cpp".
  auto [master_branch_type_info, branches_type_info] = ValidateInconsistentTypes(
    Branches | genex::views::ptr | genex::to<Vec>(), *sm, meta);

  // Ensure there is an "else" branch if the branches are
  // not exhaustive.
  // Todo: Need to investigate how to detect exhaustion.
  const auto final_not_else = Branches.Back()->Patterns[0]->To<CasePatternVariantElseAst>() == nullptr;
  RaiseIf<SppCaseBranchMissingElseError>(
    final_not_else and not meta->IgnoreMissingElseBranchForInference,
    {sm->CurrentScope}, ERR_ARGS(*this, *Branches.Back()));

  // A "case" with no "else" can finish without running any branch at all, so whatever its branches are, it is not
  // "Never": the fall-through path is reachable, and the value it produces on that path is no value. Handing back the
  // branches' type here instead is what made "case a { case b { abort() } }" crash - the inner case reads as "Never",
  // so the outer branch believes its body cannot complete and terminates the block the inner case falls through to
  // with "unreachable", which is exactly the path taken whenever "b" is false.
  //
  // @n
  // This is also half of the Todo above: a case that is not an expression yields nothing, and no "else" is the one
  // case of that which can be told apart here, because an "else" is mandatory in expression position.
  if (final_not_else) { return VoidType(PosStart()); }

  // Return the branches' return type. If there are any
  // branches, otherwise Void.
  return branches_type_info.IsEmpty()
    ? VoidType(PosStart())
    : master_branch_type_info.second;
}

auto spp::asts::CaseExpressionAst::Terminates() const
  -> bool {
  // Every branch has to terminate, and there has to be a branch
  // that always runs. Without a final "else" the case can match
  // nothing and fall straight through, so "case a { gen 1 ret }"
  // ends the scope only when "a" holds - and the statement after
  // it is reachable.
  if (Branches.IsEmpty()) { return false; }
  if (Branches.Back()->Patterns[0]->To<CasePatternVariantElseAst>() == nullptr) { return false; }
  return not genex::any_of(
    Branches, [](auto const &branch) { return not branch->Body->Terminates(); });
}

SPP_MOD_END
