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
import spp.analyse.utils.type_members;
import spp.asts.ast;
import spp.asts.defer_statement_ast;
import spp.asts.expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ice;
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
      VariableSymbol const &sym, ScopeManager const &sm) -> bool {
      // Only a local owns a value it has to answer for. A generic or a constant has no runtime existence, a
      // flow-narrowing symbol is a view of another symbol's value (which keeps the obligation), and a capture belongs
      // to the closure's environment, which is read again on every call - it is the closure value that is held to
      // being consumed. A "$" temporary a desugaring made is a local like any other: the desugaring consumes it.
      if (sym.Kind != VariableKind::Local and sym.Kind != VariableKind::Temporary) { return false; }
      if (sym.Type == nullptr or sym.MemInfo == nullptr) {
        Ice(*sym.Name, *sm.CurrentScope, "local variable has no type");
      }

      // A "!" value never exists: the code binding it cannot finish.
      if (sym.Type->IsNeverType()) { return false; }

      // A borrow points at a value that belongs to someone else, so
      // consuming it is not this scope's job.
      if (sym.MemInfo->AstBorrowed != nullptr) { return false; }
      if (sym.Type->GetConvention() != nullptr) { return false; }

      // What it holds on every path is "Copy" (a "case" narrowed it to "None"), or it was moved on the others.
      auto const &state = memory_state::StateOf(*sm.CurrentScope, sym);
      if (state.OwesNothing) { return false; }

      // Consumed on some of the paths that met and not on others, whole or in part: a path that did not take a part
      // still owns it. The witnesses kept are the first path's, so the value can read as consumed while another path
      // still owns it.
      const auto owned_on_some_path = state.MaybeInit.has_value() or not state.MaybePartialMoves.empty();

      // The value already left, whole.
      if (state.AstInitialization == nullptr and not owned_on_some_path) { return false; }

      // A holder of loans (a "&mut"-capturing closure, a borrowing generator or future) is owed like any other
      // value: it owns its environment or frame. It cannot be moved away, but a drop ends it and its loans together.

      // Copying leaves the original in place, so a copyable value
      // is never owed to anyone.
      const auto type = scopes::TypeRef::Of(*sym.Type, *sm.CurrentScope);
      if (type.Symbol == nullptr or type.Symbol->IsCopyable()) { return false; }

      // Taking every non-copyable part off a value leaves nothing
      // left to consume. A destructure is the usual way that happens,
      // and a case pattern's destructure only ever records the parts
      // it bound, never the value itself, so the parts are all there
      // is to go on.
      if (state.PartialMoves.empty() or owned_on_some_path) { return true; }
      return not regions::RegionConsumed(
        regions::Place{.Root = const_cast<VariableSymbol*>(&sym), .Steps = {}}, type, *sm.CurrentScope,
        state.PartialMoves);
    }

    /// Leaving "scope" by "exit_point", when it is the scope of a
    /// "case" branch that keeps a linear temporary, drops it.
    auto CheckKeptTemporaries(
      Scope const &scope, Ast const &exit_point,
      ScopeManager &sm, CompilerMetaData *meta) -> void {
      // Leaving the scope of a branch that keeps a "case"'s
      // temporary leaves the "case" with it unconsumed.
      auto const *const flow = memory_state::FlowOf(scope);
      if (flow == nullptr) { return; }
      for (auto const &kept : flow->KeptTemporaries) {
        if (scope.AstNode != kept.Branch) { continue; }
        RaiseTemporaryNotConsumed(
          *kept.Temporary, exit_point, "This branch keeps it and leaves here, so nothing owns it after", sm, meta);
      }
    }
  }
}

auto spp::analyse::utils::linear_utils::CheckDeferredForScope(
  Scope const &scope, Ast const &exit_point, const StrView exit_what,
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  // Reverse order: the statements run last-registered-first,
  // so a value deferred after another is released first.
  for (auto i = scope.Deferred.size(); i > 0uz; --i) {
    scope.Deferred[i - 1uz]->CheckAtExit(exit_point, exit_what, sm, meta);
  }
}

auto spp::analyse::utils::linear_utils::CheckScopeExit(
  Scope const &scope, Ast const &exit_point, const StrView exit_what,
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  //
  using errors::SppLinearValueNotConsumedError;

  for (auto const *sym : scope.GetAllVarSymbols(true)) {
    // A destructor needs the whole value to form the "self"
    // it is given, so one that has had a part taken out of
    // it and never put back can no longer be destroyed. It
    // is only here, where there is no longer anywhere to
    // repair it, that the value is stranded.
    drop_utils::CheckDestructorStillReachable(*sym, exit_point, sm, meta);

    if (not IsLive(*sym, sm)) { continue; }

    // Marked against the symbol's own name rather than against
    // whatever initialized it. A parameter is initialized by
    // the whole "name: Type" ast, so pointing at that underlines
    // the type as well, which reads as though the type were at
    // fault.
    const auto def = static_cast<Ast const*>(sym->Name.get());

    // Held in locals so the views handed to the error outlive
    // it.
    const auto sym_name = sym->Name->ToString();
    const auto type_name = sym->Type->WithoutGns()->ToString();
    Raise<SppLinearValueNotConsumedError>(ERR_ARGS(*def, exit_point, StrView(sym_name), StrView(type_name), exit_what));
  }
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToFn(
  Ast const &exit_point, const StrView exit_what,
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  // Outside a function there is no linear obligation to
  // discharge: a module-level constant outlives every scope
  // that reads it.
  if (meta->EnclosingFnScope == nullptr) { return; }

  // Running a scope's deferred statements marks what they
  // take, which is right for the path being left but wrong
  // for everything after it: stage 8 walks statements in
  // order rather than following branches, so the state is
  // put back once the exit is checked, or the code after
  // the branch a "ret" sits in would read as though the
  // deferred releases had already happened.
  const auto saved = memory_state::SnapshotFlow(*sm.CurrentScope);
  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    CheckDeferredForScope(*scope, exit_point, exit_what, sm, meta);
    CheckScopeExit(*scope, exit_point, exit_what, sm, meta);
    CheckKeptTemporaries(*scope, exit_point, sm, meta);
    if (scope == meta->EnclosingFnScope) { break; }
  }
  memory_state::RestoreSnapshot(saved);
}

auto spp::analyse::utils::linear_utils::OwnedAtSuspend(
  Ast const &suspend_point, ScopeManager &sm,
  CompilerMetaData *meta) -> Vec<Vec<OwnedValue>> {
  // As an exit is checked, on a snapshot put back afterwards: the
  // "defer"s destroying the frame here runs take what they take,
  // and whatever is still owned after them is the frame's to drop.
  // The path itself goes on past the suspension untouched.
  auto owned = Vec<Vec<OwnedValue>>();
  if (meta->EnclosingFnScope == nullptr) { return owned; }
  const auto saved = memory_state::SnapshotFlow(*sm.CurrentScope);

  // Destroying the frame ends every borrow there is: what holds one in the frame goes with it, and the caller gave up
  // whatever was lent out to it (a yield) before the frame could be destroyed. What the "defer"s run and drop is
  // checked against what is moved and initialised, not against borrows that no longer exist.
  memory_state::LoansOf(*sm.CurrentScope).clear();

  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    CheckDeferredForScope(*scope, suspend_point, "is destroyed at this \"gen\"", sm, meta);
    auto values = Vec<Pair<VariableSymbol*, Vec<Vec<spp::utils::InternedId>>>>();
    for (auto *const sym : scope->GetAllVarSymbols(true)) {
      if (not IsLive(*sym, sm)) { continue; }

      // Consumed on only some of the paths here, a value has no valid way on - not used ("inconsistently
      // initialised"), not given a new value, not left at its scope's end (E94) - so no program reaching this keeps
      // it.
      auto const &state = memory_state::StateOf(*sm.CurrentScope, *sym);
      if (state.MaybeInit.has_value() or not state.MaybePartialMoves.empty()) { continue; }

      // A part moved out is named by the fields walked to it. Todo: one taken through a method's result (an
      // opaque step) names no field, so the value it came out of is not dropped at all yet.
      auto moved = Vec<Vec<spp::utils::InternedId>>();
      auto opaque = false;
      for (auto const &move : state.PartialMoves) {
        auto &path = moved.emplace_back();
        for (auto const &step : move.Of->Steps) {
          opaque = opaque or step.Opaque;
          path.emplace_back(step.Name);
        }
      }
      if (opaque) { continue; }

      // Destroying the frame destroys what is left of it, which a part's own "drop" cannot be run on (E106 here,
      // as at a scope's end): putting the part back later does not reach a destruction at this suspension.
      drop_utils::CheckDestructorStillReachable(*sym, suspend_point, sm, meta);
      values.emplace_back(sym, std::move(moved));
    }

    // Newest first, as a type drops its fields: by where each is declared, a table having no order of its own.
    genex::actions::sort(values, std::greater{}, [](auto const &value) { return value.first->Name->PosStart(); });
    auto &scope_owned = owned.emplace_back();
    for (auto &[sym, moved] : values) { scope_owned.push_back({.Name = sym->Name, .Moved = std::move(moved)}); }
    if (scope == meta->EnclosingFnScope) { break; }
  }
  memory_state::RestoreSnapshot(saved);
  return owned;
}

auto spp::analyse::utils::linear_utils::CheckLiveUpToLoop(
  Ast const &exit_point, const StrView exit_what, const std::size_t num_exits,
  const bool has_skip, ScopeManager &sm, CompilerMetaData *meta) -> void {
  //
  auto loops_seen = 0uz;
  const auto saved = memory_state::SnapshotFlow(*sm.CurrentScope);

  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    // An iterable loop is rewritten into a conditional one
    // before this stage, so matching the conditional form
    // covers both.
    const auto is_loop = scope->AstNode != nullptr
      and AstAs<LoopConditionalExpressionAst>(scope->AstNode) != nullptr;

    if (is_loop) {
      // The loop after the ones being exited is the one a
      // trailing "skip" continues, so its own scope survives
      // this statement.
      loops_seen += 1;
      if (loops_seen > num_exits) { break; }
    }

    CheckDeferredForScope(*scope, exit_point, exit_what, sm, meta);
    CheckScopeExit(*scope, exit_point, exit_what, sm, meta);
    CheckKeptTemporaries(*scope, exit_point, sm, meta);
    if (is_loop and loops_seen == num_exits and not has_skip) { break; }
    if (scope == meta->EnclosingFnScope) { break; }
  }

  memory_state::RestoreSnapshot(saved);
}

auto spp::analyse::utils::linear_utils::CheckOverwrite(
  VariableSymbol const &sym, Ast const &site,
  const StrView site_what, ScopeManager &sm) -> void {
  //
  using errors::SppLinearValueNotConsumedError;
  if (not IsLive(sym, sm)) { return; }

  // Held in locals so the views handed to the error outlive
  // it.
  const auto sym_name = sym.Name->ToString();
  const auto type_name = sym.Type->WithoutGns()->ToString();
  Raise<SppLinearValueNotConsumedError>(ERR_ARGS(*sym.Name, site, StrView(sym_name), StrView(type_name), site_what));
}

auto spp::analyse::utils::linear_utils::CheckOverwritePart(
  VariableSymbol const &sym, ExpressionAst &part, Ast const &site,
  const StrView site_what, ScopeManager &sm, CompilerMetaData *meta) -> void {
  //
  using errors::SppLinearValueNotConsumedError;

  // A "Copy" part is never owed.
  if (sym.MemInfo == nullptr or sym.Type == nullptr) { Ice(part, *sm.CurrentScope, "assigned variable has no type"); }
  const auto part_ref = part.InferTypeRef(&sm, meta);
  if (part_ref.Symbol == nullptr or part_ref.Symbol->IsCopyable()) { return; }

  // Through a borrow ("self.name = .." with "&mut self"),
  // see "CheckOverwriteThroughBorrow".
  if (sym.MemInfo->AstBorrowed != nullptr or sym.Type->GetConvention() != nullptr) {
    CheckOverwriteThroughBorrow(part, sm, meta);
    return;
  }
  const auto part_name = part.ToString();
  const auto type_name = part_ref.Symbol->FqName()->WithoutGns()->ToString();

  // Of a value this scope owns, only a part still standing
  // is owed (writing into a moved value is refused where it
  // is checked), unless the moves taken off the value on
  // every path account for it.
  auto const &state = memory_state::StateOf(*sm.CurrentScope, sym);
  if (sym.Kind != VariableKind::Local or state.AstInitialization == nullptr) { return; }
  const auto place = regions::PlaceOf(part, *sm.CurrentScope);
  if (not place.has_value()) { return; }
  if (regions::RegionConsumed(*place, part_ref, *sm.CurrentScope, state.PartialMoves)) { return; }
  Raise<SppLinearValueNotConsumedError>(ERR_ARGS(part, site, StrView(part_name), StrView(type_name), site_what));
}

auto spp::analyse::utils::linear_utils::IsLinearTemporary(
  ExpressionAst &expr, ScopeManager &sm,
  CompilerMetaData *meta) -> bool {
  // A value naming a place belongs to that place; a borrow
  // belongs to whatever it borrows; a value made at compile
  // time never exists at run time.
  if (meta != nullptr and meta->InCompTimeValue) { return false; }
  if (regions::PlaceOf(expr, *sm.CurrentScope).has_value()) { return false; }
  const auto ref = expr.InferTypeRef(&sm, meta);
  return not ref.IsNever and not ref.IsBorrowed() and ref.Symbol != nullptr and not ref.Symbol->IsCopyable();
}

auto spp::analyse::utils::linear_utils::RaiseTemporaryNotConsumed(
  ExpressionAst &temp, Ast const &site, const StrView site_what,
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  //
  using errors::SppLinearValueNotConsumedError;

  // Held in a local so the view handed to the error outlives
  // it.
  const auto type_name = temp.InferTypeRef(&sm, meta).Symbol->FqName()->WithoutGns()->ToString();
  Raise<SppLinearValueNotConsumedError>(ERR_ARGS(temp, site, StrView(type_name), site_what));
}

auto spp::analyse::utils::linear_utils::CheckOverwriteThroughBorrow(
  ExpressionAst &part, ScopeManager &sm,
  CompilerMetaData *meta) -> void {
  // The value written over belongs to whoever is borrowed
  // from, and cannot be taken first - moving out of a borrow
  // is refused - so writing over a non-"Copy" one always drops
  // it. It has to be exchanged instead, which hands the old
  // value back ("std::mem::ops::replace").
  using errors::SppLinearValueNotConsumedError;
  const auto part_ref = part.InferTypeRef(&sm, meta);
  if (part_ref.Symbol == nullptr or part_ref.Symbol->IsCopyable()) { return; }

  // An abstract type ("Self" in a default method:
  // "self@ = source.clone()" in "Clone::clone_from") says
  // nothing about what is dropped until a class is chosen,
  // so the template is let through. Each class that inherits
  // the method gets a copy with "Self" pinned to it, checked
  // here like any other body: it is that class's value that
  // is dropped.
  auto const *const part_scope = part_ref.Symbol->LinkedScope;
  if (part_scope != nullptr and not type_members::GetUnimplementedAbstractMethods(*part_scope).empty()) { return; }
  const auto type_name = part_ref.Symbol->FqName()->WithoutGns()->ToString();

  // In a copy made for a call, the template let this through,
  // so it is the call that chose a type to drop: reported
  // there, the outermost call to reach it, with the write
  // as context.
  auto const *sub = asts::FunctionPrototypeAst::AnalysingGnSubstitution();
  while (sub != nullptr and sub->RequestedBy != nullptr and sub->RequestedBy->RequiredAt != nullptr) {
    sub = sub->RequestedBy;
  }
  if (sub != nullptr and sub->RequiredAt != nullptr and sub->RequiredIn != nullptr) {
    Raise<SppLinearValueNotConsumedError>(ERR_ARGS(part, *sub->RequiredAt, StrView(type_name)));
  }
  Raise<SppLinearValueNotConsumedError>(ERR_ARGS(part, StrView(type_name)));
}
