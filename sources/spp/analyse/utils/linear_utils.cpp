module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.linear_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.type_members;
import spp.asts.ast;
import spp.asts.defer_statement_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import genex;
import std;

namespace spp::analyse::utils::linear_utils {
  namespace {
    using Saved = Vec<Pair<
      Shared<scopes::VariableSymbol>,
      mem_info_utils::MemoryInfoSnapshot>>;

    /**
     * Snapshot every symbol in the scopes an early exit is about to be checked against. Running a scope's deferred
     * statements marks what they take, which is right for the path being left but wrong for everything after it: stage 8
     * walks statements in order rather than following branches, so without this the code after the branch a "ret" sits
     * in would read as though the deferred releases had already happened.
     */
    auto SnapshotFrom(scopes::Scope const *from, scopes::Scope const *boundary) -> Saved {
      auto saved = Saved();
      for (auto const *scope = from; scope != nullptr; scope = scope->Parent) {
        for (auto *sym : scope->AllVarSymbols(true)) {
          saved.EmplaceBack(sym->SharedFromThis<scopes::VariableSymbol>(), sym->MemInfo->Snapshot());
        }
        if (scope == boundary) { break; }
      }
      return saved;
    }

    auto RestoreFrom(Saved const &saved) -> void {
      for (auto const &[sym, snapshot] : saved) { sym->MemInfo->FillFromSnapshot(snapshot); }
    }

    /**
     * Whether this symbol still owns a value that nothing has consumed. S++ ownership is linear: a value of a
     * non-@c Copy type must be used exactly once, so a symbol reaching the end of its scope while still holding one is
     * an error.
     * @param sym The symbol being checked.
     * @param sm The scope manager, positioned where the symbol's type resolves from.
     * @return Whether the symbol still owns an unconsumed value.
     */
    auto IsLive(
      scopes::VariableSymbol const &sym,
      scopes::ScopeManager &sm)
      -> bool {
      // A symbol that never owned a value has nothing to answer for:
      // an unbound generic, a compile-time constant (which has no
      // runtime existence), or a symbol with no type to reason about.
      if (sym.IsGeneric) { return false; }

      // A flow-narrowing symbol is a view of another symbol's value,
      // typed as whatever a pattern matched. It shares the storage
      // rather than owning it, so the obligation stays with the symbol
      // it narrows.
      if (sym.IsFlowNarrowing) { return false; }
      if (sym.Type == nullptr or sym.MemInfo == nullptr) { return false; }
      if (sym.MemInfo->AstCompTime != nullptr) { return false; }

      // Todo: A "$" name is a desugaring temporary - the iterator behind a "loop ... in", the subject a destructure was
      //  bound to, the slot an early return writes through - and none of it is written by the programmer, so reporting it
      //  blames code nobody can fix. Exempting it is a real gap rather than a nicety: "$_iter" holds an iterator that
      //  genuinely goes unconsumed. The desugarings have to be made linear-correct, and then this goes away.
      if (sym.Name != nullptr and sym.Name->Val.starts_with("$")) { return false; }

      // A borrow points at a value that belongs to someone else, so
      // consuming it is not this scope's job.
      if (spp::get<0>(sym.MemInfo->AstBorrowed) != nullptr) { return false; }
      if (sym.Type->GetConvention() != nullptr) { return false; }

      // The value already left, whole.
      if (spp::get<0>(sym.MemInfo->AstInitialization) == nullptr) { return false; }

      // A symbol holding escaping borrows cannot be moved at all: the borrow rules forbid it, so that the borrow cannot
      // outlive what it points at. Asking linearity to consume it would demand a move the language prohibits, which
      // leaves no way to write the value at all. It owns nothing to account for in any case - what it holds is borrows,
      // and those belong to whoever they point at. A "&mut"-capturing closure is the usual shape.
      //
      // Todo: A generator handle is a container of escaping borrows too, and it *does* own its coroutine frame, so this
      //  exempts a real leak. Frames are not reachable through the lexical scope ends anyway, and need their own answer.
      if (not sym.MemInfo->AstContainedEscapingBorrows.IsEmpty()) { return false; }

      // Copying leaves the original in place, so a copyable value is
      // never owed to anyone.
      const auto type_sym = sm.CurrentScope->GetTypeSymbol(sym.Type.get());
      if (type_sym == nullptr or type_sym->IsCopyable()) { return false; }

      // Taking every non-copyable attribute off a value leaves nothing
      // of it to consume. The list has to be non-empty for this to mean
      // anything: a type whose attributes are all copyable has no
      // attribute that can be moved off, and would otherwise read as
      // consumed from the moment it was created. Such a type is
      // consumed by being destructured instead.
      if (sym.MemInfo->AstPartialMoves.IsEmpty()) { return true; }

      const auto owner = sym.Name->ToString();
      for (auto const &attr : type_members::GetAllAttrs(*sym.Type, sm)) {
        const auto attr_type_sym = spp::get<1>(attr);
        if (attr_type_sym == nullptr or attr_type_sym->IsCopyable()) { continue; }

        // Same string-prefix comparison the overlap checks use: a move
        // of "a" covers "a.b", and a move of "a.b" covers "a.b" itself.
        const auto region = owner + "." + spp::get<0>(attr)->ToString();
        const auto covered = genex::any_of(
          sym.MemInfo->AstPartialMoves, [&region](auto const *pm) { return region.starts_with(pm->ToString()); });
        if (not covered) { return true; }
      }

      return false;
    }
  }
}

auto spp::analyse::utils::linear_utils::CheckDeferredForScope(
  scopes::Scope const &scope,
  asts::Ast const &exit_point,
  const StrView exit_what,
  scopes::ScopeManager &sm)
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
          {sm.CurrentScope}, ERR_ARGS(*stmt, *where_moved, name->Val, exit_what));
      }

      // The same thing one branch at a time. A "case" leaves the
      // state of its first branch behind, so a value consumed
      // only in a later branch reads as live here while the
      // inconsistency flag is what remembers the disagreement -
      // and a deferred expression cannot be conditional,
      // because there is no flag at runtime to make it so.
      if (sym->MemInfo->IsInconsistentlyMoved.has_value()) {
        const auto pair = *sym->MemInfo->IsInconsistentlyMoved;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
          {sm.CurrentScope}, ERR_ARGS(*stmt, *pair.first, *pair.second, "moved"));
      }

      if (sym->MemInfo->IsInconsistentlyPartiallyMoved.has_value()) {
        const auto pair = *sym->MemInfo->IsInconsistentlyPartiallyMoved;
        Raise<errors::SppInconsistentlyInitializedMemoryUseError>(
          {sm.CurrentScope}, ERR_ARGS(*stmt, *pair.first, *pair.second, "partially moved"));
      }

      sym->MemInfo->MovedBy(exit_point, sm.CurrentScope);
      sym->MemInfo->AstPartialMoves.Clear();
    }
  }
}

auto spp::analyse::utils::linear_utils::CheckScopeExit(
  scopes::Scope const &scope,
  asts::Ast const &exit_point,
  const StrView exit_what,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta)
  -> void {
  //
  using errors::SppLinearValueNotConsumedError;

  for (auto const *sym : scope.AllVarSymbols(true)) {
    if (not IsLive(*sym, sm)) { continue; }

    // The subject of a surrounding "case ... of" that takes it has already been given up by the time a branch runs,
    // even though the mark itself is not made until the branches are done. Leaving a branch early is not what
    // abandoned it.
    //
    // Matched by name rather than by symbol, because a pattern that narrows the subject adds a flow-typed symbol of
    // its own to the branch scope - same name, same storage, narrower type - and that one is what a check inside the
    // branch actually finds.
    if (meta != nullptr and meta->CaseConsumedSubject != nullptr and sym->Name != nullptr
      and *sym->Name == *meta->CaseConsumedSubject) { continue; }

    // A symbol declared after the point control leaves from does not hold anything yet: a "ret" part-way through a
    // scope is reached before the "let"s below it ever run. Stage 7 fills the initialization ast in for every symbol
    // in the scope up front, well before any of that is known, so the two are told apart by where they are written.
    // A closing brace sits after everything in its scope, so this never excludes anything from an ordinary scope end.
    if (sym->Name != nullptr and sym->Name->PosStart() > exit_point.PosStart()) { continue; }

    // Marked against the symbol's own name rather than against
    // whatever initialized it. A parameter is initialized by the
    // whole "name: Type" ast, so pointing at that underlines the
    // type as well, which reads as though the type were at fault.
    const auto def = static_cast<asts::Ast const*>(sym->Name.get());

    // Held in locals so the views handed to the error outlive it.
    const auto sym_name = sym->Name->ToString();
    const auto type_name = sym->Type->WithoutGenerics()->ToString();
    // Todo: Migration aid, to be removed once the standard library is linear-clean. The compiler stops at the first
    //  error, which makes a several-hundred-site migration a rebuild per site; "SPP_LINEAR_SURVEY" reports every
    //  linear finding in one run instead. Analysis continues on state the error would normally have halted, so later
    //  stages are not to be trusted under it - it is for reading the list, not for building.
    if (std::getenv("SPP_LINEAR_SURVEY") != nullptr) {
      try {
        Raise<SppLinearValueNotConsumedError>(
          {sm.CurrentScope}, ERR_ARGS(*def, exit_point, StrView(sym_name), StrView(type_name), exit_what));
      }
      catch (errors::SemanticError const &e) { std::cerr << "LINEAR|" << e.what() << "\n"; }
      continue;
    }
    Raise<SppLinearValueNotConsumedError>(
      {sm.CurrentScope}, ERR_ARGS(*def, exit_point, StrView(sym_name), StrView(type_name), exit_what));
  }
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToFunction(
  asts::Ast const &exit_point,
  const StrView exit_what,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta)
  -> void {
  // Outside a function there is no linear obligation to discharge:
  // a module-level constant outlives every scope that reads it.
  if (meta->EnclosingFunctionScope == nullptr) { return; }

  const auto saved = SnapshotFrom(sm.CurrentScope, meta->EnclosingFunctionScope);
  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    CheckDeferredForScope(*scope, exit_point, exit_what, sm);
    CheckScopeExit(*scope, exit_point, exit_what, sm, meta);
    if (scope == meta->EnclosingFunctionScope) { break; }
  }
  RestoreFrom(saved);
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToLoop(
  asts::Ast const &exit_point,
  const StrView exit_what,
  const std::size_t num_exits,
  const bool has_skip,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta)
  -> void {
  //
  auto loops_seen = std::size_t{0};
  const auto saved = SnapshotFrom(sm.CurrentScope, meta->EnclosingFunctionScope);

  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    // An iterable loop is rewritten into a conditional one before
    // this stage, so matching the conditional form covers both.
    const auto is_loop = scope->AstNode != nullptr
      and AstAs<asts::LoopConditionalExpressionAst>(scope->AstNode) != nullptr;

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

  RestoreFrom(saved);
}
