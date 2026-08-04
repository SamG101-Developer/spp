module;
#include <spp/macros.hpp>

module spp.asts.case_expression_branch_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

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

auto spp::asts::CaseExpressionBranchAst::PosStart() const
  -> std::size_t {
  // Use the op or first pattern
  return Op ? Op->PosStart() : Patterns.Front()->PosStart();
}

auto spp::asts::CaseExpressionBranchAst::PosEnd() const
  -> std::size_t {
  // // Use the final pattern.
  return Patterns.Back()->PosEnd();
}

auto spp::asts::CaseExpressionBranchAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast, carrying over the desugaring marker.
  auto cloned = MakeUnique<CaseExpressionBranchAst>(
    AstClone(Op),
    AstCloneVec(Patterns),
    AstClone(Guard),
    AstClone(Body));
  cloned->_ForIterLoopYield = _ForIterLoopYield;
  return cloned;
}

auto spp::asts::CaseExpressionBranchAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Op).append(" ");
  SPP_STRING_EXTEND(Patterns, ", ");
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_APPEND(Guard).append(Guard ? " " : "");
  SPP_STRING_APPEND(Body);
  SPP_STRING_END;
}

auto spp::asts::CaseExpressionBranchAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Create a scope for the branch - this is where destructures of patterns will reside.
  auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
    "case-branch", {}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);

  // Analyse the patterns, ensuring comparison methods exist is needed.
  for (auto const &p : Patterns) {
    p->Stage7_AnalyseSemantics(sm, meta);
  }

  // Ensure the functions exist for the comparisons (whichever op is used except "is").
  // Todo: Is thick mocking okay? Conventions had to be removed from LHS, idk about RHS though.
  if (Op.get() and Op->TokenType != lex::SppTokenType::KW_IS) {
    for (auto const &p : Patterns) {
      const auto pe = p->To<CasePatternVariantExpressionAst>();
      const auto bin_ast = MakeUnique<BinaryExpressionAst>(
        MakeUnique<ObjectInitializerAst>(AstClone(meta->CaseCondition->InferType(sm, meta)->WithoutConvention()),
                                         nullptr),
        AstClone(Op),
        MakeUnique<ObjectInitializerAst>(AstClone(pe->Expr->InferType(sm, meta)->WithoutConvention()), nullptr));
      bin_ast->Stage7_AnalyseSemantics(sm, meta);
    }
  }

  // Analyse the guard and body.
  if (Guard) { Guard->Stage7_AnalyseSemantics(sm, meta); }
  Body->Stage7_AnalyseSemantics(sm, meta);

  // Exit the scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Move into the branch's scope.
  sm->MoveToNextScope();

  // Check the patterns, guard and body.
  for (auto const &p : Patterns) { p->Stage8_CheckMemory(sm, meta); }
  if (Guard) { Guard->Stage8_CheckMemory(sm, meta); }
  Body->Stage8_CheckMemory(sm, meta);

  // Move out of the branch's scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Combine the case expression with the pattern to determine if this branch should be taken, at compile-time.
  sm->MoveToNextScope();
  for (auto const &pattern : Patterns) {
    pattern->Stage9_CompTimeResolve(sm, meta);

    // Determine if this branch is not a match (false).
    const auto cmp_pat_bool = meta->CmpResult ? meta->CmpResult->To<BooleanLiteralAst>() : nullptr;
    if (cmp_pat_bool == nullptr or not cmp_pat_bool->IsTrue()) {
      sm->ExhaustScope();
      continue;
    }

    // Check with the branch guard if it exists.
    if (Guard != nullptr) {
      Guard->Stage9_CompTimeResolve(sm, meta);
      const auto cmp_guard_bool = meta->CmpResult ? meta->CmpResult->To<BooleanLiteralAst>() : nullptr;
      if (not cmp_guard_bool or not cmp_guard_bool->IsTrue()) {
        sm->ExhaustScope();
        continue;
      }
    }

    // At this point, the correct branch has been identified, so resolve the body.
    Body->Stage9_CompTimeResolve(sm, meta);
    sm->MoveOutOfCurrentScope();
    return;
  }

  // None of the patterns on this branch matched.
  meta->CmpResult = nullptr;
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // Generate the branch architecture. Start by defining blocks
  // for the branch's "body" and "next" (after body) zones.
  sm->MoveToNextScope();
  const auto uid = "." + spp::utils::Uid(this);
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
    // The guard is user-written and may assume the pattern actually matched (eg. "is Some(v) and v.foo()" reading
    // "v" as a genuine payload) or have observable side effects, so it must only run once the pattern is known to
    // have matched. Branch first, and only evaluate the guard in the block reached exclusively when "cond" was
    // true, rather than eagerly ANDing it into "cond" and evaluating it unconditionally.
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

  // For a desugared iterable loop, reaching this branch means
  // the generator yielded, so the enclosing loop counts as
  // having been entered and its "else" block must not run.
  if (_ForIterLoopYield and not meta->LlvmLoopStack.IsEmpty()) {
    if (const auto entered_flag = meta->LlvmLoopStack.Back().EnteredFlag; entered_flag != nullptr) {
      ctx->Builder.CreateStore(llvm::ConstantInt::getTrue(*ctx->Context), entered_flag);
    }
  }

  // Generate the body. As this is an expression, an llvm value
  // is returned, which can then be used in the PHI node system.
  auto llvm_val = Body->Stage11_CodeGen(sm, meta, ctx);
  const auto incoming_bb = ctx->Builder.GetInsertBlock();

  // Sometimes, a type is returned from a branch that is part of the variant type on the lhs. For example, a Opt[T]
  // might receive a Some[T] in one branch, and a None in another. In this case the member value has to be tagged and
  // copied into the variant's payload (a bit-cast cannot express that).
  if (meta->AssignmentTarget != nullptr and meta->AssignmentTargetType != nullptr and llvm_val != nullptr) {
    llvm_val = codegen::CoerceToVariant(
      llvm_val, *meta->AssignmentTargetType,
      *Body->InferType(sm, meta), *sm->CurrentScope,
      "case.branch.variant" + uid, ctx);
  }

  // Add the value generated from the branch's body into the PHI
  // node of the "meta" context. This will then be pulled by the
  // parent "case" AST. Given the branch doesn't terminate (return),
  // we branch back to the "end" block of the "case" expression.
    if (meta->LlvmPhi != nullptr) { meta->LlvmPhi->addIncoming(llvm_val, incoming_bb); }
  if (not incoming_bb->hasTerminator()) {
    ctx->Builder.CreateBr(meta->LlvmEndBB);
  }

  // Set the current point to the "next" zone, so that the next
  // branch can be added in the right place. Move out of the
  // branch's scope.
  ctx->Builder.SetInsertPoint(next_bb);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::CaseExpressionBranchAst::MarkForIterLoopYield()
  -> void {
  _ForIterLoopYield = true;
}

auto spp::asts::CaseExpressionBranchAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Forward type inference to the body.
  return Body->InferType(sm, meta);
}

auto spp::asts::CaseExpressionBranchAst::_CodegenCombinePatterns(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx) const
  -> llvm::Value* {
  // If there is only one pattern, generate its condition directly.
  // Otherwise, collect all the pattern conditions and combine them with OR. The guard (if any) is deliberately not
  // folded in here - see Stage11_CodeGen, which branches on this result before deciding whether to evaluate it.
  auto llvm_combined_pattern = Patterns.Front()->Stage11_CodeGen(sm, meta, ctx);
  for (auto const &pattern : Patterns | genex::views::ptr | genex::views::drop(1)) {
    const auto llvm_pattern = pattern->Stage11_CodeGen(sm, meta, ctx);
    llvm_combined_pattern = ctx->Builder.CreateOr(llvm_combined_pattern, llvm_pattern);
  }
  return llvm_combined_pattern;
}

SPP_MOD_END
