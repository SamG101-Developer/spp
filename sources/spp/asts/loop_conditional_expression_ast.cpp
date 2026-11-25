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
import spp.analyse.utils.common_errors;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_join;
import spp.analyse.utils.type_predicates;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_drop;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::utils::drop_utils, AddDropsOn);
use_alias(spp::analyse::utils::drop_utils, DropEdge);
use_alias(spp::analyse::utils::drop_utils, DropsOn);
use_alias(spp::analyse::utils::drop_utils, SetDropsOn);
use_alias(spp::analyse::scopes, ScopeBlockName);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::LoopConditionalExpressionAst::LoopConditionalExpressionAst(
  decltype(TokLoop) &&tok_loop,
  decltype(Cond) &&cond,
  decltype(Body) &&body,
  decltype(ElseBlock) &&else_block) :
  LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
  Cond(std::move(cond)) {
}

spp::asts::LoopConditionalExpressionAst::~LoopConditionalExpressionAst() = default;

auto spp::asts::LoopConditionalExpressionAst::PosStart() const -> std::size_t {
  // Use the "loop" token.
  return TokLoop->PosStart();
}

auto spp::asts::LoopConditionalExpressionAst::PosEnd() const -> std::size_t {
  // Use the condition.
  return Cond->PosEnd();
}

auto spp::asts::LoopConditionalExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, carrying over the desugaring marker.
  auto cloned = MakeUnique<LoopConditionalExpressionAst>(
    AstClone(TokLoop),
    AstClone(Cond),
    AstClone(Body),
    AstClone(ElseBlock));
  cloned->EndsWithLoop = EndsWithLoop;
  cloned->LoweredFrom = LoweredFrom;
  cloned->_LoopExitTypeInfo = _LoopExitTypeInfo;
  cloned->_FirstExitSite = _FirstExitSite;
  cloned->_Scope = _Scope;
  return cloned;
}

auto spp::asts::LoopConditionalExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokLoop).append(" ");
  SPP_STRING_APPEND(Cond).append(" ");
  SPP_STRING_APPEND(Body).append("\n");
  SPP_STRING_APPEND(ElseBlock);
  SPP_STRING_END;
}

auto spp::asts::LoopConditionalExpressionAst::Stage7_AnalyseSemantics(
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
  common_errors::RequirePrimary(*Cond, *sm);

  // Check the loop condition is boolean.
  if (not type_predicates::IsTypeBool(Cond->InferTypeRef(sm, meta), *sm->CurrentScope)) {
    const auto cond_ty = Cond->InferType(sm, meta);
    Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Cond, *cond_ty, "loop"));
  }

  // The loop's "exit"s, and its "else" once there is one, agree on
  // one type as a "case"'s branches do: onto what the loop is
  // assigned to, if anything ("type_join::TypeJoin").
  auto join = type_join::TypeJoin(
    meta->AssignmentTargetType, sm->CurrentScope, meta->AssignmentTarget.get());

  // Set the loop level information into the "meta" object.
  {
    const auto _meta_guard = MetaGuard(meta);
    const auto depth = meta->LoopCurrentDepth;
    meta->LoopCurrentDepth += 1;
    meta->LoopCurrentAst = this;
    meta->LoopJoins.resize(depth + 1);
    meta->LoopJoins[depth] = &join;

    // The body never yields the loop's value ("exit" does), so
    // it is not what the loop is being assigned to - as its
    // code generation already treats it.
    meta->ClearAssignmentTarget();

    // What the condition bound where it is true holds in the body.
    expr_utils::MarkValueDiscarded(*Body);
    Body->SetBoundOnEntry(expr_utils::BoundWhenTrue(*Cond));
    Body->Stage7_AnalyseSemantics(sm, meta);
  }

  // A block leaves its final statement's value to whoever wrote
  // the block, and the loop takes none: an iteration's value is
  // discarded, as every other statement's is.
  if (not Body->Members.empty()) { expr_utils::DiscardValue(*Body, Body->Members.size() - 1, *sm, meta); }

  // Analyse the else block if it exists: the loop's value when the
  // condition ends it, so one more value for a loop with "exit"s.
  // A loop with none is "Void", which the "else" must be too.
  if (ElseBlock != nullptr) {
    ElseBlock->Stage7_AnalyseSemantics(sm, meta);
    if (not join.Empty()) {
      auto const *const final_member = ElseBlock->Body->FinalMember();
      join.Give(
        final_member != nullptr ? *static_cast<Ast const*>(final_member) : *static_cast<Ast const*>(ElseBlock.get()),
        ElseBlock->InferType(sm, meta), *sm->CurrentScope, ElseBlock->Body->Diverges(sm, meta));
    }
  }
  _LoopExitTypeInfo = Nullopt;
  _FirstExitSite = nullptr;
  if (not join.Empty()) {
    _LoopExitTypeInfo = join.Solve();
    join.InstantiateFnValues(*_LoopExitTypeInfo, sm, meta);
    const auto first = genex::find_if(join.Values(), [](auto const &v) { return not v.Diverges; });
    _FirstExitSite = first != join.Values().end() ? first->Site : join.Values().front().Site;
  }

  // Exit the loop scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::LoopConditionalExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  using memory_state::MemoryState;
  using memory_state::ScopeSnapshot;

  // Move into the loop scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // What the condition binds is the iteration's own, never an
  // outer symbol the loop's head is over: an enclosing loop's
  // earlier pass may have left a state for it.
  drop_utils::ForgetIterationBindings(*sm->CurrentScope);

  // Where the body's walk starts, for each pass after the first.
  const auto body_start = sm->Position();

  mem_utils::ValidateSymbolMemory(*Cond, *TokLoop, *sm, meta);

  // What is visible as the loop is entered, and what the body's
  // flow has touched by then: the symbols a loop's head is over.
  const auto pre_loop_state = memory_state::SnapshotSymbols(sm->CurrentScope->GetAllVarSymbols(), *sm->CurrentScope);

  // What lives outside the loop: the symbols visible at it, and
  // every one the body's flow had touched by then (hidden by a
  // shadowing "let" or not). These are the states joined after it.
  auto outer_syms = Vec<VariableSymbol*>();
  auto seen = Set<VariableSymbol const*>();
  for (auto const &[sym, _] : pre_loop_state.Symbols) {
    if (seen.insert(sym.get()).second) { outer_syms.emplace_back(sym.get()); }
  }
  for (auto const &[sym, _] : pre_loop_state.States) {
    if (seen.insert(sym).second) { outer_syms.emplace_back(const_cast<VariableSymbol*>(sym)); }
  }

  // This loop's frame in the body's flow, by position: a loop
  // nested in the body pushes its own after it.
  auto &flow = *memory_state::FlowOf(*sm->CurrentScope);
  const auto frame_at = flow.Loops.size();
  flow.Loops.emplace_back();

  // One pass over the condition and body: one iteration, from the
  // state the symbols are in now. The "skip"s and "exit"s it meets
  // are its own: a later pass begins from more than this one did,
  // so its edges stand for this one's.
  // The condition's "is" bindings are the iteration's: each
  // iteration binds them afresh, so they have to be dead by its
  // end (the body's end here, a "skip" where it is). A temporary
  // the condition made is not: the condition consumes it, or the
  // loop ends with it.
  // The condition's temporaries live through the body (its "is"
  // bindings view them), so they end with the iteration; when the
  // condition ends the loop instead, what it left them holding is
  // dropped there (none of what a logical operator's right operand
  // made, ended there: "flag and next() is Some(v)").
  // An "exit" or "skip" here aimed at an enclosing loop ("exit exit",
  // "exit skip") files itself in that loop's frame, on every pass over
  // this body: each pass begins from the frames as this loop found
  // them, so only the last (settled) pass's stand.
  auto outer_marks = Vec<Arr<std::size_t, 3>>();
  for (auto i = 0uz; i < frame_at; ++i) {
    auto const &outer = flow.Loops[i];
    outer_marks.push_back({outer.SkipStates.size(), outer.ExitStates.size(), outer.ExitStmts.size()});
  }
  const auto cut_back = [](auto &list, const std::size_t to) {
    list.erase(list.begin() + static_cast<std::ptrdiff_t>(to), list.end());
  };
  auto held_at_cond_end = Vec<drop_utils::OwnedValue>();
  const auto run_iteration = [&](ScopeManager &walk) {
    flow.Loops[frame_at].SkipStates.clear();
    flow.Loops[frame_at].ExitStates.clear();
    flow.Loops[frame_at].ExitStmts.clear();
    for (auto i = 0uz; i < frame_at; ++i) {
      cut_back(flow.Loops[i].SkipStates, outer_marks[i][0]);
      cut_back(flow.Loops[i].ExitStates, outer_marks[i][1]);
      cut_back(flow.Loops[i].ExitStmts, outer_marks[i][2]);
    }
    drop_utils::ForgetIterationBindings(*walk.CurrentScope);
    const auto iteration = drop_utils::TemporaryRegion(*walk.CurrentScope);
    BinaryExpressionAst::MarkCondition(*Cond);
    Cond->Stage8_CheckMemory(&walk, meta);
    held_at_cond_end = iteration.Held(*TokLoop, walk, meta, BinaryExpressionAst::HeldWhenTrue(*Cond));
    Body->Stage8_CheckMemory(&walk, meta);
    if (not Body->Diverges(&walk, meta)) {
      auto const &end = Body->TokR != nullptr ? static_cast<Ast const&>(*Body->TokR) : static_cast<Ast const&>(*Body);
      SetDropsOn(*this, DropEdge::kIteration, iteration.End(end, walk, meta));
    }
  };

  // The state the loop was entered with, for each outer symbol, and
  // the loans held then: where the first iteration begins, and the
  // condition can end the loop before any.
  using HeadStates = Vec<Pair<VariableSymbol*, MemoryState>>;
  auto entry = HeadStates();
  for (auto *const sym : outer_syms) { entry.emplace_back(sym, memory_state::StateOf(*sm->CurrentScope, *sym)); }
  const auto entry_snapshot = memory_state::SnapshotSymbols(outer_syms, *sm->CurrentScope);
  const auto entry_loans = memory_state::LoansOf(*sm->CurrentScope);
  run_iteration(*sm);

  // Every later iteration begins from one of the ways back round, as
  // a pass over the body leaves them: the body's end (unless it never
  // finishes: it always leaves by "exit" or "ret", or is "!") and each
  // "skip". The second iteration's head ("back") is what the first pass
  // left on them, joined; each later pass joins in what it leaves, so
  // the head only grows more uncertain, and each pass checks the body
  // against any number of iterations before it. It settles when a pass
  // adds nothing: a pass that does not settle makes some outer symbol's
  // state or the loans more uncertain, and each can become so only a
  // few times, so the bound is a compiler bug's, not a program's.
  auto back = Opt<HeadStates>();
  auto back_loans = memory_state::LoanSet();
  const auto body_diverges = Body->Diverges(sm, meta);
  if (not body_diverges or flow.Loops[frame_at].Skips != 0) {
    const auto max_passes = 8 * (outer_syms.size() + 1);
    for (auto pass = 1uz;; ++pass) {
      if (pass > max_passes) {
        Ice(*this, *sm->CurrentScope, std::format("loop memory state still changing after {} passes", max_passes));
      }

      // The ways back round this pass left: "nullptr" is the state now,
      // the body's end.
      auto edges = Vec<Pair<Ast*, ScopeSnapshot const*>>();
      if (not body_diverges) { edges.emplace_back(Body.get(), nullptr); }
      for (auto const &[skip, skip_state] : flow.Loops[frame_at].SkipStates) {
        edges.emplace_back(const_cast<Ast*>(skip), &skip_state);
      }
      if (edges.empty()) { break; }

      // Joined into the head so far: the entry's, after the first pass.
      // An iteration after the first begins where every way into the
      // head meets, the entry included: a value one way holds and
      // another does not is dropped on the way holding it (below), so
      // it is held on none.
      auto next = back.value_or(entry);
      for (auto i = 0uz; i < next.size(); ++i) {
        auto *const sym = next[i].first;
        auto states = Vec<Pair<MemoryState, Ast*>>();
        states.emplace_back(next[i].second, back.has_value() ? edges[0].first : TokLoop.get());
        for (auto const &[edge, snapshot] : edges) {
          states.emplace_back(
            snapshot == nullptr ? memory_state::StateOf(*sm->CurrentScope, *sym) : memory_state::StateIn(*snapshot, *sym),
            edge);
        }
        next[i].second = memory_state::JoinAll(std::move(states), *sym, *sm->CurrentScope);
      }
      const auto loans_on = [&](Pair<Ast*, ScopeSnapshot const*> const &edge) {
        return edge.second == nullptr ? memory_state::LoansOf(*sm->CurrentScope) : edge.second->Loans;
      };
      auto next_loans = back.has_value() ? back_loans : loans_on(edges[0]);
      for (auto e = back.has_value() ? 0uz : 1uz; e < edges.size(); ++e) {
        memory_state::JoinLoans(next_loans, loans_on(edges[e]), edges[0].first, edges[e].first);
      }

      const auto settled = back.has_value() and memory_state::SameLoans(next_loans, back_loans) and genex::all_of(
        genex::views::zip(*back, next), [](auto const &x) {
          return memory_state::SameShape(get<0>(x).second, get<1>(x).second);
        });
      if (settled) { break; }

      // Begin the next iteration from the joined head.
      back = std::move(next);
      back_loans = std::move(next_loans);
      for (auto const &[sym, state] : *back) { memory_state::StateOf(*sm->CurrentScope, *sym) = state; }
      memory_state::LoansOf(*sm->CurrentScope) = back_loans;
      auto tm = sm->NewManagerAt(body_start);
      run_iteration(tm);
    }
  }

  // The head's join: what the entry, the body's end and each "skip"
  // hold that another way in does not is dropped on that way.
  SetDropsOn(*this, DropEdge::kEntry, {});
  SetDropsOn(*this, DropEdge::kBackEdge, {});
  if (back.has_value()) {
    auto head_paths = Vec<drop_utils::JoinPath>{{&entry_snapshot, TokLoop.get(), TokLoop.get()}};
    if (not body_diverges) { head_paths.push_back({nullptr, Body.get(), Body.get()}); }
    for (auto const &[skip, skip_state] : flow.Loops[frame_at].SkipStates) {
      head_paths.push_back({&skip_state, const_cast<Ast*>(skip), skip});
    }
    auto head_drops = drop_utils::JoinPaths(
      outer_syms, head_paths, drop_utils::OwnersAround(*sm->CurrentScope, meta), *sm, meta);
    SetDropsOn(*this, DropEdge::kEntry, std::move(head_drops[0]));
    auto next_drops = 1uz;
    if (not body_diverges) { SetDropsOn(*this, DropEdge::kBackEdge, std::move(head_drops[next_drops++])); }
    for (auto const &[skip, _] : flow.Loops[frame_at].SkipStates) {
      if (auto *const stmt = const_cast<Ast*>(skip)->To<LoopControlFlowStatementAst>(); stmt != nullptr) {
        AddDropsOn(*stmt, DropEdge::kAfterExit, std::move(head_drops[next_drops]));
      }
      next_drops += 1;
    }
  }

  const auto frame = std::move(flow.Loops[frame_at]);
  flow.Loops.pop_back();

  // A condition other than "true" can end the loop, at its head: the
  // state any iteration (or none) begins with, the entry joined with
  // the settled head of the later ones. An "else" block runs from
  // there.
  const auto cond_bool = Cond->To<BooleanLiteralAst>();
  const auto cond_can_end = cond_bool == nullptr or not cond_bool->IsTrue();
  if (cond_can_end) {
    for (auto i = 0uz; i < entry.size(); ++i) {
      auto *const sym = entry[i].first;
      auto states = Vec<Pair<MemoryState, Ast*>>();
      states.emplace_back(entry[i].second, TokLoop.get());
      if (back.has_value()) { states.emplace_back((*back)[i].second, Body.get()); }
      memory_state::StateOf(*sm->CurrentScope, *sym) = memory_state::JoinAll(std::move(states), *sym, *sm->CurrentScope);
    }
    auto loans = entry_loans;
    if (back.has_value()) { memory_state::JoinLoans(loans, back_loans, TokLoop.get(), Body.get()); }
    memory_state::LoansOf(*sm->CurrentScope) = std::move(loans);
  }

  // What the condition held only where it was true is not held on the
  // path where it ended the loop.
  if (cond_can_end) {
    for (auto *const sym : BinaryExpressionAst::HeldWhenTrue(*Cond)) {
      memory_state::StateOf(*sm->CurrentScope, *sym).MovedBy(*TokLoop);
    }
  }

  // Check the else block if it exists.
  if (ElseBlock != nullptr) {
    ElseBlock->Stage8_CheckMemory(sm, meta);
  }

  // The state after the loop is the merge of every path out of
  // it, as for the branches of a "case": the head, where the
  // condition ends it (through the "else" block, whose end then
  // stands for it), and every "exit", with the state it was
  // reached in.
  //
  // Each path's state for one symbol: "nullptr" is the state it
  // is in now, which needs no restoring.
  auto paths = Vec<Pair<Ast const*, ScopeSnapshot const*>>();
  if (cond_can_end) {
    const auto through = ElseBlock != nullptr ? static_cast<Ast const*>(ElseBlock.get()) : TokLoop.get();
    paths.emplace_back(through, nullptr);
  }
  for (auto const &[exit_tok, state] : frame.ExitStates) { paths.emplace_back(exit_tok, &state); }

  // A loop nothing leaves ("loop true { }") has no state after it.
  if (paths.empty()) {
    sm->MoveOutOfCurrentScope();
    return;
  }
  // A value of a scope around the loop held on only some of the
  // paths out is dropped on each path still holding it, at its end:
  // the condition's (index 0, when it can end the loop) or an
  // "exit"'s.
  // Every path's state, joined. An "exit" released what the scopes
  // it left held, on its own edge.
  const auto owners = drop_utils::OwnersAround(*sm->CurrentScope, meta);
  auto join_paths = Vec<drop_utils::JoinPath>();
  for (auto const &[end, state] : paths) { join_paths.push_back({state, const_cast<Ast*>(end), end}); }
  auto path_drops = drop_utils::JoinPaths(outer_syms, join_paths, owners, *sm, meta);
  // The value that ends with the loop is dropped on every way out, whatever else
  // each path holds, and is gone after it: the loans it holds end here.
  auto *const ends_with_loop = EndsWithLoop != nullptr ? sm->CurrentScope->FindVarSymbol(EndsWithLoop.get()) : nullptr;
  if (ends_with_loop != nullptr) {
    auto const *owner_scope = sm->CurrentScope->Parent;
    while (owner_scope != nullptr and not genex::contains(owner_scope->GetOwners(), ends_with_loop)) {
      owner_scope = owner_scope->Parent;
    }
    for (auto i = 0uz; owner_scope != nullptr and i < paths.size(); ++i) {
      const auto state = paths[i].second == nullptr
        ? memory_state::StateOf(*sm->CurrentScope, *ends_with_loop)
        : memory_state::StateIn(*paths[i].second, *ends_with_loop);
      if (auto value = drop_utils::OwnedOnPath(*ends_with_loop, state, *owner_scope, *paths[i].first, *sm, meta)) {
        path_drops[i].emplace_back(std::move(*value));
      }
    }
    memory_state::StateOf(*sm->CurrentScope, *ends_with_loop).MovedBy(*TokLoop);
  }
  for (auto &drops : path_drops) { drop_utils::SortNewestFirst(drops); }
  const auto first_exit = cond_can_end ? 1uz : 0uz;
  SetDropsOn(*this, DropEdge::kCondEnd, cond_can_end ? std::move(path_drops[0]) : Vec<drop_utils::OwnedValue>());

  // The condition that ended the loop matched no "is": its
  // temporaries (not its bindings) still hold what it left them, a
  // failed match's value included.
  if (cond_can_end) { AddDropsOn(*this, DropEdge::kCondEnd, std::move(held_at_cond_end)); }
  if (first_exit + frame.ExitStmts.size() != paths.size()) {
    Ice(*this, *sm->CurrentScope, "a loop's exit states and exit statements are out of step");
  }
  for (auto i = 0uz; i < frame.ExitStmts.size(); ++i) {
    if (const auto stmt = frame.ExitStmts[i]->To<LoopControlFlowStatementAst>(); stmt != nullptr) {
      AddDropsOn(*stmt, DropEdge::kAfterExit, std::move(path_drops[first_exit + i]));
    }
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
  if (ends_with_loop != nullptr) { memory_state::ReleaseLoans(loans, *ends_with_loop); }

  // Exit the loop scope.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::LoopConditionalExpressionAst::Stage11_CodeGen(
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

  // The "else" block runs whenever the condition ends the loop,
  // however many iterations it took first: it is the loop's value
  // when no "exit" gives one. Without one (or drops to run on
  // that path first, "kCondEnd"), the condition ends the
  // loop at its end block.
  const auto loop_else_bb = ElseBlock != nullptr or not DropsOn(*this, DropEdge::kCondEnd).empty()
    ? llvm::BasicBlock::Create(*ctx->Context, "loop.else" + uid, func)
    : loop_end_bb;
  codegen::EmitOuterDrops(DropsOn(*this, DropEdge::kEntry), *this, sm, meta, ctx);
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
    meta->LlvmLoopStack.emplace_back(
      loop_cond_bb, loop_end_bb, phi, sm->CurrentScope,
      phi != nullptr ? MakeShared<TypeRef>(ret_ref) : nullptr, codegen::TemporaryDepth(ctx));
    meta->AssignmentTargetType = ret_type;

    // Generate the condition. This block is branched back to
    // at the end of every iteration, so the condition is
    // re-evaluated each time round.
    // The temporaries the condition makes ("loop f(&mk())") are its own: each evaluation drops them once its
    // value is made, before either way out.
    ctx->Builder.SetInsertPoint(loop_cond_bb);
    const auto cond_temporaries = codegen::TemporaryDepth(ctx);
    const auto llvm_cond = Cond->Stage11_CodeGen(sm, meta, ctx);
    codegen::EndTemporaryRegion(cond_temporaries, *sm->CurrentScope, {}, Cond.get(), sm, meta, ctx);
    const auto cond_end_bb = ctx->Builder.GetInsertBlock();
    ctx->Builder.CreateCondBr(llvm_cond, loop_body_bb, loop_else_bb);

    // Generate the loop body block. The body itself never yields
    // the loop's value (only "exit" does), so the assignment
    // target is cleared to stop the final body statement being
    // treated as a yielded value.
    ctx->Builder.SetInsertPoint(loop_body_bb);
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AssignmentTarget = nullptr;
      meta->LlvmAssignmentTarget = nullptr;
      Body->Stage11_CodeGen(sm, meta, ctx);
    }
    codegen::EmitOwnedDrops(*sm->CurrentScope, DropsOn(*this, DropEdge::kIteration), this, sm, meta, ctx);
    codegen::EmitOuterDrops(DropsOn(*this, DropEdge::kBackEdge), *this, sm, meta, ctx);
    if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      ctx->Builder.CreateBr(loop_cond_bb);
    }

    if (loop_else_bb != loop_end_bb) {
      // The condition ended the loop: the else block gives its
      // value, and the loop is left, so what its own scope
      // deferred runs.
      ctx->Builder.SetInsertPoint(loop_else_bb);
      auto else_val = ElseBlock != nullptr ? ElseBlock->Stage11_CodeGen(sm, meta, ctx) : nullptr;
      if (phi != nullptr and ElseBlock != nullptr) {
        else_val = codegen::CoerceToSlot(
          else_val, ret_ref, ElseBlock->InferTypeRef(sm, meta), *sm, "loop.else.coerce" + uid, ctx);
      }
      codegen::EmitScopeExit(sm->CurrentScope->Parent, sm, meta, ctx);
      codegen::EmitOuterDrops(DropsOn(*this, DropEdge::kCondEnd), *this, sm, meta, ctx);
      const auto else_end_bb = ctx->Builder.GetInsertBlock();
      if (not else_end_bb->hasTerminator()) {
        if (phi != nullptr) {
          phi->addIncoming(else_val != nullptr ? else_val : llvm::UndefValue::get(phi->getType()), else_end_bb);
        }
        ctx->Builder.CreateBr(loop_end_bb);
      }
    }
    else {
      // The failing condition targets the end block directly. A loop with a value and no "else" is E117 unless its
      // condition is "true", so the edge is never taken; the phi still needs a value for it.
      if (phi != nullptr) { phi->addIncoming(llvm::UndefValue::get(phi->getType()), cond_end_bb); }
    }

    // Finish the loop expression.
  }
  sm->MoveOutOfCurrentScope();
  ctx->Builder.SetInsertPoint(loop_end_bb);
  return phi;
}

auto spp::asts::LoopConditionalExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  IMPORT_UTILS;
  using generate::common_types_precompiled::NeverAt;

  // A "loop true" with no exit statements returns "Never".
  const auto cond_lit = Cond->To<BooleanLiteralAst>();
  if (cond_lit != nullptr and cond_lit->TokBool->TokenType == lex::SppTokenType::KW_TRUE) {
    // Check the internal flow controls.
    if (not _LoopExitTypeInfo.has_value()) {
      return NeverAt(PosStart());
    }
  }

  // A condition other than "true" can end the loop without an
  // "exit", and then the "else" block is its value: one with a
  // value from its "exit"s needs one (as a "case" does).
  auto type = LoopExpressionAst::InferType(sm, meta);
  const auto cond_is_true = cond_lit != nullptr and cond_lit->TokBool->TokenType == lex::SppTokenType::KW_TRUE;
  if (not cond_is_true and ElseBlock == nullptr and _LoopExitTypeInfo.has_value()
    and not meta->IgnoreMissingElseBranchForInference) {
    auto const &agreed = *_LoopExitTypeInfo;
    const auto exit_ref = TypeRef::Of(*agreed.Type, *agreed.TypeScope);
    RaiseIf<SppLoopMissingElseError>(
      not agreed.Diverges and not exit_ref.IsNever and not type_predicates::IsTypeVoid(exit_ref, *sm->CurrentScope),
      ERR_ARGS(
        LoweredFrom != nullptr ? *LoweredFrom : *static_cast<Ast const*>(this),
        _FirstExitSite != nullptr ? *_FirstExitSite : *static_cast<Ast const*>(this)));
  }
  return type;
}

auto spp::asts::LoopConditionalExpressionAst::InferTypeRef(
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

SPP_MOD_END
