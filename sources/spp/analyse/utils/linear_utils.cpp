module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.linear_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.asts.ast;
import spp.asts.defer_statement_ast;
import spp.asts.identifier_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import genex;
import std;

namespace spp::analyse::utils::linear_utils {
  namespace {
    /**
     * Whether this symbol still owns a value that nothing has consumed. S++ ownership is linear: a value of a
     * non-@c Copy type must be used exactly once, so a symbol reaching the end of its scope while still holding one is
     * an error.
     * @param sym The symbol being checked.
     * @param sm The scope manager, positioned where the symbol's type resolves from.
     * @return Whether the symbol still owns an unconsumed value.
     */
    auto IsLive(
      VariableSymbol const &sym,
      ScopeManager const &sm)
      -> bool {
      // Only a local owns a value it has to answer for. A generic or a constant has no runtime existence, a
      // flow-narrowing symbol is a view of another symbol's value (which keeps the obligation), and a capture belongs
      // to the closure's environment, which is read again on every call - it is the closure value that is held to
      // being consumed.
      //
      // Todo: A "$" temporary is exempted too - the iterator behind a "loop ... in", the subject a destructure was
      //  bound to, the slot an early return writes through - since reporting it blames code nobody can fix. That is a
      //  real gap rather than a nicety: "$_iter" holds an iterator that genuinely goes unconsumed. The desugarings
      //  have to be made linear-correct, and then "Temporary" is checked here too.
      if (sym.Kind != VariableKind::Local) { return false; }
      if (sym.Type == nullptr or sym.MemInfo == nullptr) { return false; }

      // A "!" value never exists: the code binding it cannot finish.
      if (sym.Type->IsNeverType()) { return false; }

      // A borrow points at a value that belongs to someone else, so
      // consuming it is not this scope's job.
      if (spp::get<0>(sym.MemInfo->AstBorrowed) != nullptr) { return false; }
      if (sym.Type->GetConvention() != nullptr) { return false; }

      // Consumed on some branches of a "case" and not on others. The state carried on past the "case" is only the
      // first branch's, so it can read as consumed while another path still owns the value. The partial-move flag is
      // not used: it compares the move sites themselves, so two branches that each take the same attribute differ.
      const auto owned_on_some_path =
        sym.MemInfo->IsInconsistentlyMoved.has_value() or
        sym.MemInfo->IsInconsistentlyInitialized.has_value();

      // The value already left, whole.
      if (spp::get<0>(sym.MemInfo->AstInitialization) == nullptr and not owned_on_some_path) { return false; }

      // A symbol holding escaping borrows cannot be moved at all:
      // the borrow rules forbid it, so that the borrow cannot
      // outlive what it points at. Asking linearity to consume
      // it would demand a move the language prohibits, which
      // leaves no way to write the value at all. It owns nothing
      // to account for in any case - what it holds is borrows,
      // and those belong to whoever they point at. A
      // "&mut"-capturing closure is the usual shape.
      //
      // Todo: A generator handle is a container of escaping borrows too, and it *does* own its coroutine frame, so this
      //  exempts a real leak. Frames are not reachable through the lexical scope ends anyway, and need their own
      //  answer.
      if (not sym.MemInfo->AstContainedEscapingBorrows.IsEmpty()) { return false; }

      // Copying leaves the original in place, so a copyable value
      // is never owed to anyone.
      const auto type_sym = sm.CurrentScope->GetTypeSymbol(sym.Type.get());
      if (type_sym == nullptr or type_sym->IsCopyable()) { return false; }

      // Taking every non-copyable part off a value leaves nothing
      // left to consume. A destructure is the usual way that happens,
      // and a case pattern's destructure only ever records the parts
      // it bound, never the value itself, so the parts are all there
      // is to go on.
      if (sym.MemInfo->AstPartialMoves.IsEmpty() or owned_on_some_path) { return true; }
      return not regions::RegionConsumed(
        Vec<IdentifierAst*>{sym.Name.get()}, *type_sym, *sm.CurrentScope, sym.MemInfo->AstPartialMoves);
    }
  }
}

auto spp::analyse::utils::linear_utils::CheckDeferredForScope(
  Scope const &scope,
  Ast const &exit_point,
  const StrView exit_what,
  ScopeManager &sm)
  -> void {
  // Reverse order: the statements run last-registered-first,
  // so a value deferred after another is released first.
  for (auto i = scope.Deferred.Len(); i > 0uz; --i) {
    const auto stmt = scope.Deferred[i - 1uz];

    // Resolved by name against this scope, so that an
    // instantiation's own copies of the symbols are the ones
    // marked.
    for (auto const &name : stmt->Consumed) {
      const auto sym = scope.GetVarSymbolOutermost(*name).first;
      if (sym == nullptr) { continue; }

      // Running a deferred expression consumes what it names,
      // so reaching this exit with the value already gone means
      // it is consumed twice on this path. Raise memory error.
      if (const auto where_moved = spp::get<0>(sym->MemInfo->AstMoved); where_moved != nullptr) {
        Raise<errors::SppDeferConsumesMovedValueError>(
          {sm.CurrentScope}, ERR_ARGS(*stmt, *where_moved, exit_point, name->Val, exit_what));
      }

      // The same thing one branch at a time. A "case" leaves the
      // state of its first branch behind, so a value consumed
      // only in a later branch reads as live here while the
      // inconsistency flag is what remembers the disagreement -
      // and a deferred expression cannot be conditional,
      // because there is no flag at runtime to make it so.
      mem_utils::RaiseIfInconsistentlyMoved(*sym, *stmt, sm.CurrentScope);

      sym->MemInfo->MovedBy(exit_point, sm.CurrentScope);
    }
  }
}

auto spp::analyse::utils::linear_utils::CheckScopeExit(
  Scope const &scope,
  Ast const &exit_point,
  const StrView exit_what,
  ScopeManager &sm,
  CompilerMetaData *meta)
  -> void {
  //
  using errors::SppLinearValueNotConsumedError;

  for (auto const *sym : scope.AllVarSymbols(true)) {
    // The subject of a surrounding "case ... of" that takes it has already been given up by the time a branch runs,
    // even though the mark itself is not made until the branches are done. Leaving a branch early is not what
    // abandoned it.
    //
    // Matched by name rather than by symbol, because a pattern that narrows the subject adds a flow-typed symbol of
    // its own to the branch scope - same name, same storage, narrower type - and that one is what a check inside the
    // branch actually finds.
    if (meta != nullptr and sym->Name != nullptr and genex::any_of(
      meta->CaseConsumedSubjects, [&sym](auto const &subject) { return *sym->Name == *subject; })) { continue; }

    // A symbol declared after the point control leaves from does not hold anything yet: a "ret" part-way through a
    // scope is reached before the "let"s below it ever run. Stage 7 fills the initialization ast in for every symbol
    // in the scope up front, well before any of that is known, so the two are told apart by where they are written.
    // A closing brace sits after everything in its scope, so this never excludes anything from an ordinary scope end.
    if (sym->Name != nullptr and sym->Name->PosStart() > exit_point.PosStart()) { continue; }

    // A destructor needs the whole value to form the "self"
    // it is given, so one that has had a part taken out of
    // it and never put back can no longer be destroyed. It
    // is only here, where there is no longer anywhere to
    // repair it, that the value is stranded.
    drop_utils::CheckDestructorStillReachable(*sym, exit_point, sm, meta);

    if (not IsLive(*sym, sm)) { continue; }

    // Marked against the symbol's own name rather than against
    // whatever initialized it. A parameter is initialized by the
    // whole "name: Type" ast, so pointing at that underlines the
    // type as well, which reads as though the type were at fault.
    const auto def = static_cast<Ast const*>(sym->Name.get());

    // Held in locals so the views handed to the error outlive it.
    const auto sym_name = sym->Name->ToString();
    const auto type_name = sym->Type->WithoutGenerics()->ToString();
    Raise<SppLinearValueNotConsumedError>(
      {sm.CurrentScope}, ERR_ARGS(*def, exit_point, StrView(sym_name), StrView(type_name), exit_what));
  }
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToFunction(
  Ast const &exit_point,
  const StrView exit_what,
  ScopeManager &sm,
  CompilerMetaData *meta)
  -> void {
  // Outside a function there is no linear obligation to discharge:
  // a module-level constant outlives every scope that reads it.
  if (meta->EnclosingFunctionScope == nullptr) { return; }

  // Running a scope's deferred statements marks what they take, which is right for the path being left but wrong for
  // everything after it: stage 8 walks statements in order rather than following branches, so the state is put back
  // once the exit is checked, or the code after the branch a "ret" sits in would read as though the deferred releases
  // had already happened.
  const auto saved = memory_state::SnapshotScopes(sm.CurrentScope, meta->EnclosingFunctionScope);
  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    CheckDeferredForScope(*scope, exit_point, exit_what, sm);
    CheckScopeExit(*scope, exit_point, exit_what, sm, meta);
    if (scope == meta->EnclosingFunctionScope) { break; }
  }
  memory_state::RestoreSnapshot(saved);
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToLoop(
  Ast const &exit_point,
  const StrView exit_what,
  const std::size_t num_exits,
  const bool has_skip,
  ScopeManager &sm,
  CompilerMetaData *meta)
  -> void {
  //
  auto loops_seen = 0uz;
  const auto saved = memory_state::SnapshotScopes(sm.CurrentScope, meta->EnclosingFunctionScope);

  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    // An iterable loop is rewritten into a conditional one before
    // this stage, so matching the conditional form covers both.
    const auto is_loop = scope->AstNode != nullptr
      and AstAs<LoopConditionalExpressionAst>(scope->AstNode) != nullptr;

    if (is_loop) {
      // The loop after the ones being exited is the one a trailing
      // "skip" continues, so its own scope survives this statement.
      loops_seen += 1;
      if (loops_seen > num_exits) { break; }
    }

    CheckDeferredForScope(*scope, exit_point, exit_what, sm);
    CheckScopeExit(*scope, exit_point, exit_what, sm, meta);
    if (is_loop and loops_seen == num_exits and not has_skip) { break; }
    if (scope == meta->EnclosingFunctionScope) { break; }
  }

  memory_state::RestoreSnapshot(saved);
}

auto spp::analyse::utils::linear_utils::CheckOverwrite(
  VariableSymbol const &sym,
  Ast const &site,
  const StrView site_what,
  ScopeManager &sm)
  -> void {
  //
  using errors::SppLinearValueNotConsumedError;
  if (not IsLive(sym, sm)) { return; }

  // Held in locals so the views handed to the error outlive it.
  const auto sym_name = sym.Name->ToString();
  const auto type_name = sym.Type->WithoutGenerics()->ToString();
  Raise<SppLinearValueNotConsumedError>(
    {sm.CurrentScope}, ERR_ARGS(*sym.Name, site, StrView(sym_name), StrView(type_name), site_what));
}
