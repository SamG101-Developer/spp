module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.linear_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.defer_statement_ast;
import spp.asts.function_prototype_ast;
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
     * The type of one part of a type, and the scope that type resolves in: an attribute by name, or an element of a
     * tuple or an array by index. Nothing when the type has no such part.
     */
    auto IndividualPartType(
      asts::TypeAst const &type,
      scopes::Scope const &scope,
      Str const &step)
      -> Pair<Shared<asts::TypeAst>, scopes::Scope const*> {
      // Find the part the step names, which is an attribute's own
      // name for a struct and an element's index for a tuple or
      // an array.
      for (auto const &[part_step, _, part_type, part_sym, part_scope] : type_members::GetAllParts(type, scope)) {
        if (part_step == step) { return {part_type, part_scope}; }
      }

      // Failsafe, should never be reached.
      return {nullptr, nullptr};
    }

    /**
     * The type of the place @p steps names after @p count of its steps, and the scope that type resolves in. Step
     * zero is the symbol itself, so a @p count of zero is the symbol's own type and a count of @c {steps.Len() - 1}
     * is the place the whole path names. Nothing when any step along the way has no such part.
     */
    auto DescendToPart(
      Shared<asts::TypeAst> root_type,
      scopes::Scope const &root_scope,
      Vec<Str> const &steps,
      const std::size_t count)
      -> Pair<Shared<asts::TypeAst>, scopes::Scope const*> {
      auto part_type = std::move(root_type);
      auto const *part_scope = &root_scope;
      for (auto i = std::size_t{1}; i <= count and i < steps.Len(); ++i) {
        if (part_type == nullptr or part_scope == nullptr) { break; }
        auto [next_type, next_scope] = IndividualPartType(*part_type, *part_scope, steps[i]);
        part_type = std::move(next_type);
        part_scope = next_scope;
      }
      return {std::move(part_type), part_scope};
    }

    /**
     * Whether everything a place owns has been consumed, given every partial move recorded against the symbol it
     * hangs off. A move accounts for a place directly when it names the place or something containing it. A place is
     * also accounted for piecemeal, by its parts: a case pattern never marks the value it destructures as moved, only
     * each element it binds, so "case p is Outer(i=Inner(a, b), y)" records "p.i.a", "p.i.b" and "p.y", and "p.i" is
     * only covered by finding that both of its own parts were taken. The parts of a class are its attributes; the
     * parts of a tuple or an array are its elements, which a destructure records by index.
     * @param region The names of the steps of the place being accounted for, outermost first.
     * @param type The type of that place.
     * @param scope The scope @p type resolves in.
     * @param moves Every partial move recorded against the owning symbol.
     * @return Whether the place has nothing left to consume.
     */
    auto RegionConsumed(
      Vec<Str> const &region,
      asts::TypeAst const &type,
      scopes::Scope const &scope,
      Vec<asts::Ast const*> const &moves,
      Str *const unaccounted = nullptr,
      const bool descend_regardless = false)
      -> bool {
      // If there is a registered move of the entire region, then
      // a full consume has been done, so return true. Otherwise,
      // detect if a different move contains the region.
      auto touched = false;
      for (auto const *move : moves) {
        const auto relation = mem_utils::MemRegionRelate(*move, region);
        if (relation == mem_utils::MemRegionRelation::Contains) { return true; }
        if (relation == mem_utils::MemRegionRelation::ContainedBy) { touched = true; }
      }

      // If the region is not contained by any moves, then we can
      // skip individual checks, and return false here; an early
      // return optimization.
      if (not touched and not descend_regardless) {
        if (unaccounted != nullptr and unaccounted->empty()) {
          for (auto const &step : region) { *unaccounted += unaccounted->empty() ? step : "." + step; }
        }
        return false;
      }

      auto part = region;
      part.EmplaceBack(Str()); // Extra spot for temp "final" part.

      // Each part is checked on its own, under the name a destructure would have recorded it by - an attribute's own
      // name, or an element's index. A copyable part was never owed to anyone, so it never has to be accounted for.
      // If any part is unaccounted for, then nor is the value holding it.
      for (auto const &[step, _, part_type, part_sym, part_scope] : type_members::GetAllParts(type, scope)) {
        if (part_sym == nullptr or part_sym->IsCopyable()) { continue; }
        part.Back() = step;
        if (not RegionConsumed(part, *part_type, *part_scope, moves, unaccounted)) { return false; }
      }

      // Nothing left behind, so at this point we know the value
      // is empty via all its partial moves.
      return true;
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
      scopes::ScopeManager const &sm)
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

      // A closure's capture belongs to its environment, not to the body
      // that reads it: the environment owns the value and is read again
      // on every call, and it is the closure value that is held to being
      // consumed.
      if (sym.IsCapture) { return false; }
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
      if (sym.MemInfo->AstPartialMoves.IsEmpty()) { return true; }
      return not RegionConsumed(
        Vec{sym.Name->Val}, *sym.Type, *sm.CurrentScope, sym.MemInfo->AstPartialMoves);
    }

    /**
     * Raise if @p sym holds a value with a destructor that can no longer be run, because a part of it has been moved
     * out and nothing put one back. Asked of every place a recorded move reached through, not only of @p sym itself:
     * @c {o.inner.val} leaves @c {o.inner} unable to be destroyed even when @c {o} has no destructor of its own. Only
     * a type with a @c drop of its own has anything to lose here: everything else is destroyed field by field, which
     * a partial move has already done for the parts it took.
     * @param sym The symbol being checked.
     * @param exit_point The ast to report the error against.
     * @param sm The scope manager, positioned where the symbol's type resolves from.
     * @param meta Associated metadata, for resolving the destructor overload.
     */
    auto CheckDestructorStillReachable(
      scopes::VariableSymbol const &sym,
      asts::Ast const &exit_point,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *const meta)
      -> void {
      // Nothing taken out of it is nothing to put back.
      if (sym.MemInfo->AstPartialMoves.IsEmpty()) { return; }
      if (sym.Type == nullptr) { return; }

      // A borrow does not own what it points at, so the value
      // behind it is not this scope's to destroy.
      if (spp::get<0>(sym.MemInfo->AstBorrowed) != nullptr) { return; }
      if (sym.Type->GetConvention() != nullptr) { return; }

      for (auto const *move : sym.MemInfo->AstPartialMoves) {
        // A move leaves a hole in every place it reached *through*, so each of those is asked in turn: the symbol's
        // own type first, then one step further in for each name the path passes on its way. The place the move
        // landed on is not one of them - taking a whole field out hands that field's destructor to whoever received
        // it, and only taking something from inside a value strands the value's own. That is what stops the last
        // step being walked, and what makes "let x = o.inner" fine where "let x = o.inner.val" is not.
        const auto path = mem_utils::RegionPath(*move)
          | genex::views::transform([](const auto step) { return step->Val; })
          | genex::to<Vec>();

        for (auto i = 0uz; i + 1 < path.Len(); ++i) {
          const auto [part_type, part_scope] = DescendToPart(sym.Type, *sm.CurrentScope, path, i);
          if (part_type == nullptr or part_scope == nullptr) { break; }

          const auto type_sym = part_scope->GetTypeSymbol(part_type.get());
          if (type_sym == nullptr) { continue; }

          const auto destructor = drop_utils::FindDropOverload(*type_sym, sm, meta);
          if (destructor == nullptr) { continue; }

          // Held in a local so the view handed to the error
          // outlives it.
          const auto owner_name = part_type->WithoutGenerics()->ToString();
          Raise<errors::SppPartialMoveOfDestructibleValueError>(
            {sm.CurrentScope}, ERR_ARGS(exit_point, *move, *destructor->Name, StrView(owner_name)));
        }
      }
    }
  }
}

auto spp::analyse::utils::linear_utils::FirstUnaccountedPart(
  scopes::VariableSymbol const &sym,
  Vec<Str> const &region,
  scopes::ScopeManager const &sm)
  -> Str {
  // No region parts -> no unaccounted parts. Simple optimization
  // guard.
  if (sym.Type == nullptr or region.IsEmpty()) { return Str(); }

  // Walk the whole region, so that "a.b.c" lands on the type of
  // "c" and the scope that type resolves in.
  const auto [region_type, region_scope] = DescendToPart(sym.Type, *sm.CurrentScope, region, region.Len() - 1);
  if (region_type == nullptr or region_scope == nullptr) { return Str(); }

  // The caller has established that the pattern took this place
  // apart, so its parts are walked whether or not any of them
  // recorded a move.
  auto unaccounted = Str();
  auto _ = RegionConsumed(
    region, *region_type, *region_scope, sym.MemInfo->AstPartialMoves, &unaccounted, true);
  return unaccounted;
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
  asts::meta::CompilerMetaData *meta)
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
    CheckDestructorStillReachable(*sym, exit_point, sm, meta);

    if (not IsLive(*sym, sm)) { continue; }

    // Marked against the symbol's own name rather than against
    // whatever initialized it. A parameter is initialized by the
    // whole "name: Type" ast, so pointing at that underlines the
    // type as well, which reads as though the type were at fault.
    const auto def = static_cast<asts::Ast const*>(sym->Name.get());

    // Held in locals so the views handed to the error outlive it.
    const auto sym_name = sym->Name->ToString();
    const auto type_name = sym->Type->WithoutGenerics()->ToString();
    Raise<SppLinearValueNotConsumedError>(
      {sm.CurrentScope}, ERR_ARGS(*def, exit_point, StrView(sym_name), StrView(type_name), exit_what));
  }
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToFunction(
  asts::Ast const &exit_point,
  const StrView exit_what,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
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
  asts::meta::CompilerMetaData *meta)
  -> void {
  //
  auto loops_seen = 0uz;
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
