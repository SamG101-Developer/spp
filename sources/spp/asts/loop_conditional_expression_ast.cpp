module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_conditional_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.control_flow;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_predicates;
import spp.asts.boolean_literal_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
LoopConditionalExpressionAst::LoopConditionalExpressionAst(
  decltype(TokLoop) &&tok_loop,
  decltype(Cond) &&cond,
  decltype(Body) &&body,
  decltype(ElseBlock) &&else_block) :
  LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
  Cond(std::move(cond)),
  _IterDesugar(false) {
}

LoopConditionalExpressionAst::~LoopConditionalExpressionAst() = default;

auto LoopConditionalExpressionAst::PosStart() const -> std::size_t {
  // Use the "loop" token.
  return TokLoop->PosStart();
}

auto LoopConditionalExpressionAst::PosEnd() const -> std::size_t {
  // Use the condition.
  return Cond->PosEnd();
}

auto LoopConditionalExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, carrying over the desugaring marker.
  auto cloned = MakeUnique<LoopConditionalExpressionAst>(
    AstClone(TokLoop),
    AstClone(Cond),
    AstClone(Body),
    AstClone(ElseBlock));
  if (_IterDesugar) { cloned->MarkAsIterDesugar(); }
  cloned->_LoopExitTypeInfo = _LoopExitTypeInfo;
  cloned->_Scope = _Scope;
  return cloned;
}

auto LoopConditionalExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokLoop).append(" ");
  SPP_STRING_APPEND(Cond).append(" ");
  SPP_STRING_APPEND(Body).append("\n");
  SPP_STRING_APPEND(ElseBlock);
  SPP_STRING_END;
}

auto LoopConditionalExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Create the loop scope.
  auto scope_name = ScopeBlockName::FromParts(
    "loop-cond-expr", {}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // Analyse the condition expression.
  Cond->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Cond, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Cond));

  // Check the loop condition is boolean.
  if (not type_predicates::IsTypeBool(Cond->InferTypeRef(sm, meta), *sm->CurrentScope)) {
    const auto cond_ty = Cond->InferType(sm, meta);
    Raise<SppExpressionNotBooleanError>({sm->CurrentScope}, ERR_ARGS(*Cond, *cond_ty, "loop"));
  }

  // Set the loop level information into the "meta" object.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->LoopCurrentDepth += 1;
    meta->LoopCurrentAst = this;

    // The body never yields the loop's value ("exit" does), so
    // it is not what the loop is being assigned to - as its
    // code generation already treats it.
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    Body->Stage7_AnalyseSemantics(sm, meta);
    if (meta->LoopReturnTypes->contains(meta->LoopCurrentDepth - 1)) {
      _LoopExitTypeInfo = (*meta->LoopReturnTypes)[meta->LoopCurrentDepth - 1];
    }
  }

  // Analyse the else block if it exists.
  if (ElseBlock != nullptr) {
    ElseBlock->Stage7_AnalyseSemantics(sm, meta);
  }

  // Exit the loop scope.
  sm->MoveOutOfCurrentScope();
}

auto LoopConditionalExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  using memory_state::MemoryState;
  using memory_state::ScopeSnapshot;

  // Move into the loop scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Where the body's walk starts, for each pass after the first.
  const auto body_start = sm->GetCurrentIterator();

  mem_utils::ValidateSymbolMemory(*Cond, *TokLoop, *sm, meta);

  // The state the loop was entered with, for the path that
  // never runs the body.
  const auto pre_loop_state = memory_state::SnapshotSymbols(sm->CurrentScope->GetAllVarSymbols(), *sm->CurrentScope);

  // What lives outside the loop: the symbols visible at it, and every one the body's flow had touched by then (hidden
  // by a shadowing "let" or not). These are the states joined after it.
  auto outer_syms = Vec<VariableSymbol*>();
  auto seen = Set<VariableSymbol const*>();
  for (auto const &[sym, _] : pre_loop_state.Symbols) {
    if (seen.insert(sym.get()).second) { outer_syms.emplace_back(sym.get()); }
  }
  for (auto const &[sym, _] : pre_loop_state.States) {
    if (seen.insert(sym).second) { outer_syms.emplace_back(const_cast<VariableSymbol*>(sym)); }
  }

  // This loop's frame in the body's flow, by position: a loop nested in the body pushes its own after it.
  auto &flow = *memory_state::FlowOf(*sm->CurrentScope);
  const auto frame_at = flow.Loops.size();
  flow.Loops.emplace_back();

  // One pass over the condition and body: one iteration, from the
  // state the symbols are in now.
  const auto run_iteration = [&](ScopeManager &walk) {
    Cond->Stage8_CheckMemory(&walk, meta);
    Body->Stage8_CheckMemory(&walk, meta);

    // What a "skip" left moved is how the next iteration can begin
    // too, even if the body's end puts it back. Only for what lives
    // outside the loop: a "let" in the body is declared afresh.
    for (auto const &[sym, skip] : flow.Loops[frame_at].SkipMoves) {
      auto &state = memory_state::StateOf(*sm->CurrentScope, *sym);
      if (seen.contains(sym) and state.AstMoved == nullptr) {
        state.MaybeInit = memory_state::PathsDisagree{.First = const_cast<Ast*>(skip), .Other = this};
      }
    }
  };
  run_iteration(*sm);

  // Each next iteration begins with what the one before left behind,
  // until that stops changing: then every iteration has been seen. A
  // body that never reaches its end (it always leaves by "exit" or
  // "ret", or is "!") has no next iteration unless a "skip" starts
  // one. The bound only guards against a state that never settles;
  // each pass has checked another iteration by then.
  const auto body_diverges = control_flow::Diverges(*Body, sm, meta);
  if (not body_diverges or flow.Loops[frame_at].Skips != 0) {
    constexpr auto max_iterations = 8;
    for (auto iteration = 1; iteration < max_iterations; ++iteration) {
      const auto entry = memory_state::SnapshotSymbols(outer_syms, *sm->CurrentScope);
      auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
      tm.Reset(sm->CurrentScope, body_start);
      run_iteration(tm);
      const auto settled = genex::all_of(entry.Symbols, [sm](auto const &x) {
        return memory_state::SameShape(x.second, memory_state::StateOf(*sm->CurrentScope, *x.first));
      }) and memory_state::SameLoans(entry.Loans, memory_state::LoansOf(*sm->CurrentScope));
      if (settled) { break; }
    }
  }

  const auto frame = std::move(flow.Loops[frame_at]);
  flow.Loops.pop_back();

  // Check the else block if it exists.
  if (ElseBlock != nullptr) {
    ElseBlock->Stage8_CheckMemory(sm, meta);
  }

  // The state after the loop is the merge of every path out of
  // it, as for the branches of a "case". A condition other than
  // "true" can end the loop: before the first iteration (the
  // state it was entered with), or after one that reached the
  // body's end (the state now). An "else" block is where both
  // of those go, so its end stands for them. Every "exit" is a
  // path out too, with the state it was reached in.
  const auto cond_bool = Cond->To<BooleanLiteralAst>();
  const auto cond_can_end = cond_bool == nullptr or not cond_bool->IsTrue();
  const auto body_end_reached = not body_diverges or frame.Skips != 0;

  // Each path's state for one symbol: "nullptr" is the state it
  // is in now, which needs no restoring.
  auto paths = Vec<Pair<Ast const*, ScopeSnapshot const*>>();
  if (cond_can_end and ElseBlock != nullptr) { paths.emplace_back(ElseBlock.get(), nullptr); }
  if (cond_can_end and ElseBlock == nullptr) {
    if (body_end_reached) { paths.emplace_back(Body.get(), nullptr); }
    paths.emplace_back(TokLoop.get(), &pre_loop_state);
  }
  for (auto const &[exit_tok, state] : frame.ExitStates) { paths.emplace_back(exit_tok, &state); }

  // A loop nothing leaves ("loop true { }") has no state after it.
  if (paths.empty()) {
    sm->MoveOutOfCurrentScope();
    return;
  }
  for (auto *const sym : outer_syms) {
    const auto state_on = [&](auto const &path) -> MemoryState {
      if (path.second == nullptr) { return memory_state::StateOf(*sm->CurrentScope, *sym); }
      return memory_state::StateIn(*path.second, *sym);
    };

    // Every path's state, joined. An "exit" released what the
    // scopes it left held, on its own edge.
    const auto settled = [&](auto const &path) {
      return memory_state::SettleFullyTaken(state_on(path), *sym, *sm->CurrentScope);
    };
    auto joined = settled(paths[0]);
    for (auto const &path : paths | genex::views::drop(1)) {
      memory_state::Join(joined, settled(path), const_cast<Ast*>(paths[0].first), const_cast<Ast*>(path.first));
    }
    memory_state::StateOf(*sm->CurrentScope, *sym) = joined;
  }

  // The loans held after the loop: every path's out of it, joined, as for the symbols.
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  const auto loans_on = [&](auto const &path) { return path.second == nullptr ? loans : path.second->Loans; };
  auto joined_loans = loans_on(paths[0]);
  for (auto const &path : paths | genex::views::drop(1)) {
    memory_state::JoinLoans(
      joined_loans, loans_on(path), const_cast<Ast*>(paths[0].first), const_cast<Ast*>(path.first));
  }
  loans = std::move(joined_loans);

  // Exit the loop scope.
  sm->MoveOutOfCurrentScope();
}

auto LoopConditionalExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;

  // Move into the loop scope.
  sm->MoveToNextScope();

  // Determine if this loop will be yielding an expression.
  // A "Void" loop (used as a statement) and a "Never" loop
  // ("loop true" with no "exit"s) have no value to merge,
  // so no phi node. Any other loop yields its value wherever
  // it stands, not only under an assignment target: a value
  // that is produced must be used, and "f(loop .. { exit 5 })"
  // passes it straight to a call - which, without the phi,
  // left the "exit" nothing to feed and crashed the compiler.
  const auto uid = "." + Uid();
  const auto ret_type = InferType(sm, meta);
  const auto ret_ref = TypeRef::Of(*ret_type, *sm->CurrentScope);
  const auto is_expr = not type_predicates::IsTypeVoid(
    ret_ref, *sm->CurrentScope) and not ret_ref.IsNever;

  // Create the key required blocks: the condition entry
  // point, the body entry point, and the end of the loop
  // ie where to go once the loop is done.
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto loop_cond_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.cond" + uid, func);
  const auto loop_body_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.body" + uid, func);
  const auto loop_end_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.end" + uid, func);

  // As the "else" block is optional, the "not taken" branch
  // either points to the "else" block, or otherwise the end
  // block.
  const auto loop_else_bb = ElseBlock != nullptr
    ? llvm::BasicBlock::Create(*ctx->Context, "loop.else" + uid, func)
    : nullptr;

  const auto loop_not_taken_bb = ElseBlock != nullptr
    ? llvm::BasicBlock::Create(*ctx->Context, "loop.not_taken" + uid, func)
    : loop_end_bb;

  // The "else" block runs only when the loop never took an
  // iteration, so track whether the body has been entered.
  auto entered_flag = static_cast<llvm::Value*>(nullptr);
  if (ElseBlock != nullptr) {
    const auto i1_type = llvm::Type::getInt1Ty(*ctx->Context);
    entered_flag = codegen::LlvmEntryAlloca(i1_type, "loop.entered" + uid, ctx);
    ctx->Builder.CreateStore(llvm::ConstantInt::getFalse(i1_type), entered_flag);
  }
  ctx->Builder.CreateBr(loop_cond_bb);

  // The phi merges the value yielded by the "else" block
  // with the values yielded by every "exit" statement.
  auto phi = static_cast<llvm::PHINode*>(nullptr);
  if (is_expr) {
    ctx->Builder.SetInsertPoint(loop_end_bb);
    const auto llvm_phi_type = codegen::GetLlvmTypeOf(
      ret_ref, ctx);
    phi = ctx->Builder.CreatePHI(
      llvm_phi_type, 2U, "loop.phi" + uid);
  }

  // Register this loop so that nested "exit"/"skip" statements
  // can branch to the correct blocks. The stack is ordered
  // outermost-first, so "exit exit" pops 2 frames off the back
  // to find its target.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->LlvmEndBB = loop_end_bb;
    meta->LlvmPhi = phi;
    meta->LlvmLoopStack.emplace_back(loop_cond_bb, loop_end_bb, phi, entered_flag, sm->CurrentScope);
    meta->AssignmentTargetType = ret_type;

    // Generate the condition. This block is branched back to
    // at the end of every iteration, so the condition is
    // re-evaluated each time round.
    ctx->Builder.SetInsertPoint(loop_cond_bb);
    const auto llvm_cond = Cond->Stage11_CodeGen(sm, meta, ctx);
    const auto cond_end_bb = ctx->Builder.GetInsertBlock();
    ctx->Builder.CreateCondBr(llvm_cond, loop_body_bb, loop_not_taken_bb);

    // Generate the loop body block. The body itself never yields
    // the loop's value (only "exit" does), so the assignment
    // target is cleared to stop the final body statement being
    // treated as a yielded value.
    ctx->Builder.SetInsertPoint(loop_body_bb);
    if (entered_flag != nullptr and not _IterDesugar) {
      ctx->Builder.CreateStore(llvm::ConstantInt::getTrue(*ctx->Context), entered_flag);
    }
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AssignmentTarget = nullptr;
      meta->LlvmAssignmentTarget = nullptr;
      Body->Stage11_CodeGen(sm, meta, ctx);
    }
    if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      ctx->Builder.CreateBr(loop_cond_bb);
    }

    if (ElseBlock != nullptr) {
      // The condition failed: run the else block only if no
      // iteration was ever taken, otherwise leave the loop.
      ctx->Builder.SetInsertPoint(loop_not_taken_bb);
      const auto was_entered = ctx->Builder.CreateLoad(
        llvm::Type::getInt1Ty(*ctx->Context), entered_flag, "loop.was_entered" + uid);
      ctx->Builder.CreateCondBr(was_entered, loop_end_bb, loop_else_bb);
      if (phi != nullptr) { phi->addIncoming(llvm::UndefValue::get(phi->getType()), loop_not_taken_bb); }

      // Generate the else block itself.
      ctx->Builder.SetInsertPoint(loop_else_bb);
      const auto else_val = ElseBlock->Stage11_CodeGen(sm, meta, ctx);
      const auto else_end_bb = ctx->Builder.GetInsertBlock();
      if (not else_end_bb->hasTerminator()) {
        if (phi != nullptr) { phi->addIncoming(else_val, else_end_bb); }
        ctx->Builder.CreateBr(loop_end_bb);
      }
    }
    else {
      // The failing condition targets the end block directly, but the phi still needs a value for it.
      if (phi != nullptr) { phi->addIncoming(llvm::UndefValue::get(phi->getType()), cond_end_bb); }
    }

    // Finish the loop expression.
  }
  sm->MoveOutOfCurrentScope();
  ctx->Builder.SetInsertPoint(loop_end_bb);
  return phi;
}

auto LoopConditionalExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  using generate::common_types_precompiled::NeverAt;


  // A "loop true" with no exit statements returns "Never".
  const auto cond_lit = Cond->To<BooleanLiteralAst>();
  if (cond_lit != nullptr and cond_lit->TokBool->TokenType == lex::SppTokenType::KW_TRUE) {
    // Check the internal flow controls.
    if (not _LoopExitTypeInfo.has_value()) {
      return NeverAt(PosStart());
    }
  }

  return LoopExpressionAst::InferType(sm, meta);
}

auto LoopConditionalExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // A "loop true" with no exit statements is "Never"; otherwise as any loop.
  using generate::common_types_precompiled::NEVER;
  const auto cond_lit = Cond->To<BooleanLiteralAst>();
  if (cond_lit != nullptr and cond_lit->TokBool->TokenType == lex::SppTokenType::KW_TRUE
    and not _LoopExitTypeInfo.has_value()) {
    return TypeRef::Of(*NEVER, *sm->CurrentScope);
  }
  return LoopExpressionAst::InferTypeRef(sm, meta);
}

auto LoopConditionalExpressionAst::MarkAsIterDesugar() -> void {
  // Simple getter for the flag reporting this ast is a
  // transformation from the loop-iter block.
  _IterDesugar = true;
}

auto LoopConditionalExpressionAst::Terminates() const -> bool {
  // A conditional loop never terminates the scope it is
  // written in: the condition is checked before the first
  // iteration, so a loop whose body returns can still run
  // zero times and fall through to whatever follows.
  return false;
}

SPP_MOD_END
