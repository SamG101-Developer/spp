module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_control_flow_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_join;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.loop_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_defer;
import spp.lex.tokens;
import genex;
import llvm;

SPP_MOD_BEGIN
LoopControlFlowStatementAst::LoopControlFlowStatementAst(
  decltype(TokSeqExit) &&tok_seq_exit,
  decltype(TokSkip) &&tok_skip,
  decltype(Expr) &&expr) :
  TokSeqExit(std::move(tok_seq_exit)),
  TokSkip(std::move(tok_skip)),
  Expr(std::move(expr)) {
}

LoopControlFlowStatementAst::~LoopControlFlowStatementAst() = default;

auto LoopControlFlowStatementAst::PosStart() const -> std::size_t {
  // Use the first "exit" or "skip".
  return TokSeqExit.empty() ? TokSkip->PosStart() : TokSeqExit.front()->PosStart();
}

auto LoopControlFlowStatementAst::PosEnd() const -> std::size_t {
  // Use expression or "skip" or last "exit".
  return Expr ? Expr->PosEnd() : TokSkip ? TokSkip->PosEnd() : TokSeqExit.back()->PosEnd();
}

auto LoopControlFlowStatementAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto p = MakeUnique<LoopControlFlowStatementAst>(
    AstCloneVec(TokSeqExit),
    AstClone(TokSkip),
    AstClone(Expr));
  p->_ExprIsNever = _ExprIsNever;
  return p;
}

auto LoopControlFlowStatementAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(TokSeqExit, " ");
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_APPEND(TokSkip);
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto LoopControlFlowStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  using generate::common_types_precompiled::VoidAt;

  // Get the number of control flow statements, and the
  // loop's nesting level.
  const auto has_skip = TokSkip != nullptr;
  const auto num_controls = TokSeqExit.size() + (has_skip ? 1 : 0);
  const auto nested_loop_depth = meta->LoopCurrentDepth;

  // Check the depth of the loop is greater than or equal
  // to the number of control statements. Outside any loop
  // there is no loop to point at, so the statement stands in.
  RaiseIf<SppLoopTooManyControlFlowStatementsError>(
    num_controls > nested_loop_depth,
    ERR_ARGS(
      meta->LoopCurrentAst != nullptr ? static_cast<Ast const&>(*meta->LoopCurrentAst->TokLoop) : *this,
      *this, num_controls, nested_loop_depth));

  // Save and compare the loop's "exiting" type against
  // other nested loop's exit statement types.
  if (not has_skip) {
    auto expr_type = VoidAt(PosStart());

    // Analyse the expression if it is present.
    if (Expr != nullptr) {
      Expr->Stage7_AnalyseSemantics(sm, meta);
      RaiseIf<SppInvalidPrimaryExpressionError>(
        Expr and not expr_utils::IsPrimaryExprTypeValid(*Expr, *sm),
        ERR_ARGS(*Expr.get()));

      expr_type = Expr->InferType(sm, meta);
      _ExprIsNever = expr_type->IsNeverType();
    }

    // The value goes to the loop it exits, whose "exit"s and "else"
    // agree on the loop's type there ("type_join::TypeJoin"). One
    // that diverges ("!") is no value. A bare "exit" has no
    // expression, so the loop control itself is named.
    const auto depth = nested_loop_depth - num_controls;
    auto const &found_at = Expr != nullptr ? *Expr->To<Ast>() : *To<Ast>();
    if (depth < meta->LoopJoins.size() and meta->LoopJoins[depth] != nullptr) {
      meta->LoopJoins[depth]->Give(found_at, expr_type, *sm->CurrentScope, expr_type->IsNeverType());
    }
  }
}

auto LoopControlFlowStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;
  auto &loops = memory_state::FlowOf(*sm->CurrentScope)->Loops;

  // What memory looks like on the edge to the "n"th loop out (1 is
  // the innermost): the state here, with what the scopes inside
  // that loop's own scope held released, as each of their ends
  // would; the loop's own scope outlives the edge. The walk carries
  // on past this statement, so its state is put back.
  const auto state_leaving_to = [&](const std::size_t n) {
    const auto saved = memory_state::SnapshotFlow(*sm->CurrentScope);
    auto loops_seen = 0uz;
    for (auto scope = sm->CurrentScope; scope != nullptr; scope = scope->Parent) {
      const auto is_loop = AstAs<LoopConditionalExpressionAst>(scope->AstNode) != nullptr;
      if (is_loop and ++loops_seen == n) { break; }
      for (const auto sym : scope->GetAllVarSymbols(true)) {
        memory_state::ReleaseLoans(memory_state::LoansOf(*scope), *sym);
      }
    }
    auto state = memory_state::SnapshotFlow(*sm->CurrentScope);
    memory_state::RestoreSnapshot(saved);
    return state;
  };

  // A "skip" starts another iteration of the loop it continues:
  // the innermost, or with "exit"s before it, the one after the
  // loops they leave. Its state is a way into that iteration, as
  // the body's end is.
  if (TokSkip != nullptr and TokSeqExit.size() < loops.size()) {
    auto &continued = loops[loops.size() - 1 - TokSeqExit.size()];
    continued.Skips += 1;
    continued.SkipStates.emplace_back(TokSkip.get(), state_leaving_to(TokSeqExit.size() + 1));
  }

  // Check the memory state of the expression if it is present.
  // Expression is being moved into outer context, so strict
  // memory checks.
  if (Expr != nullptr) {
    Expr->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(
      *Expr, *TokSeqExit.back(), *sm, meta);
  }

  // An "exit" (with no "skip") is a path out of the loop it
  // targets, and what memory looks like here is what that loop
  // leaves behind on it. "exit exit" targets the loop two back.
  if (TokSkip == nullptr and not TokSeqExit.empty() and TokSeqExit.size() <= loops.size()) {
    loops[loops.size() - TokSeqExit.size()].ExitStates.emplace_back(
      TokSeqExit.back().get(), state_leaving_to(TokSeqExit.size()));
  }

  // Like a "ret", this leaves several scopes at once, so their
  // closing braces are never reached on this path and their own
  // checks never see it.
  const auto exit_point = TokSeqExit.empty()
    ? static_cast<Ast const*>(TokSkip.get())
    : static_cast<Ast const*>(TokSeqExit.back().get());
  linear_utils::CheckLiveUpToLoop(
    *exit_point, TokSeqExit.empty() ? "Loop skip" : "Loop exit",
    TokSeqExit.size(), TokSkip != nullptr, *sm, meta);
}

auto LoopControlFlowStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  //

  // The loop stack is ordered outermost-first, so the innermost
  // enclosing loop is the back element.
  const auto num_exits = TokSeqExit.size();
  const auto has_skip = TokSkip != nullptr;
  const auto num_loops = meta->LlvmLoopStack.size();

  // Generate the attached expression (if present) before the
  // branch, so the value is available to the phi node.
  const auto llvm_val = Expr != nullptr
    ? Expr->Stage11_CodeGen(sm, meta, ctx)
    : nullptr;

  // The target is the Nth innermost loop, N being the number of
  // "exit" tokens; a "skip" after them restarts the loop they
  // select, and a bare "skip" the innermost one. Copied, not
  // referenced: generating what the scopes deferred can generate
  // other code, whose loops grow (and move) the stack.
  const auto target = meta->LlvmLoopStack[num_loops - num_exits - (has_skip ? 1 : 0)];

  // The value goes into the loop's phi, so it is coerced into the
  // loop's type as any slot is ("exit Some(val=1)" into an
  // "Opt[S32]" loop).
  const auto coerced_val = Expr != nullptr and target.Type != nullptr and not has_skip
    ? codegen::CoerceToSlot(llvm_val, *target.Type, Expr->InferTypeRef(sm, meta), *sm, "loop.exit.coerce", ctx)
    : llvm_val;

  // The jump skips every scope end inside the loop, so what
  // those scopes deferred runs at the jump: a "skip" stays in the
  // loop's own scope, and an "exit" leaves it too. This happens
  // before the phi edge is recorded, so the incoming block is
  // whatever it left the builder inserting into.
  codegen::EmitScopeExit(
      has_skip ? target.LoopScope : target.LoopScope->Parent, sm, meta, ctx);

  // A "skip" starts the next iteration at the condition check.
  // It never has a value, so no phi node is involved.
  if (has_skip) {
    ctx->Builder.CreateBr(target.CondBB);
    return nullptr;
  }

  // An "exit" leaves the loop: its phi node collects the value
  // from this edge, and the "loop" hands it on from there.
  if (target.Phi != nullptr) {
    // A "!" value never reaches the phi, but the edge still needs
    // an operand of the phi's type.
    const auto incoming_val = coerced_val != nullptr and _ExprIsNever
      ? llvm::PoisonValue::get(target.Phi->getType())
      : coerced_val;
    const auto incoming_bb = ctx->Builder.GetInsertBlock();
    target.Phi->addIncoming(
      incoming_val != nullptr ? incoming_val : llvm::UndefValue::get(target.Phi->getType()),
      incoming_bb);
  }
  ctx->Builder.CreateBr(target.EndBB);
  return nullptr;
}

auto LoopControlFlowStatementAst::Diverges(
  ScopeManager *, CompilerMetaData *) -> bool {
  // An "exit" or a "skip" always jumps, so nothing after it
  // in this scope runs.
  return true;
}

auto LoopControlFlowStatementAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // If there is an attached expression, return its type,
  // otherwise Void.
  using generate::common_types_precompiled::VoidAt;
  return Expr != nullptr ? Expr->InferType(sm, meta) : VoidAt(PosStart());
}

auto LoopControlFlowStatementAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  using generate::common_types_precompiled::VOID;
  return Expr != nullptr
    ? Expr->InferTypeRef(sm, meta)
    : TypeRef::Of(*VOID, *sm->CurrentScope);
}

SPP_MOD_END
