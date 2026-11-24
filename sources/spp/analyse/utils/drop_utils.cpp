module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.drop_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.string_literal_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.ice;
import spp.utils.interner;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::analyse::scopes, VariableKind);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts, Ast);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, LoopConditionalExpressionAst);
use_alias(spp::asts, FunctionCallArgumentAst);
use_alias(spp::asts, FunctionPrototypeAst);
use_alias(spp::asts, GenericArgumentAst);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, StringLiteralAst);
use_alias(spp::asts::meta, CompilerMetaData);

namespace spp::analyse::utils::drop_utils {
  using overload_resolution::FnOverload;

  /// Whether "sym" still owns a value nothing has taken, on some
  /// path at least, in "state" (one path's state of it): an owned
  /// local (or "$" temporary) of a move-only type, initialised, and
  /// not taken apart whole.
  static auto IsOwnedIn(
    VariableSymbol const &sym, memory_state::MemoryState const &state, ScopeManager const &sm) -> bool;

  /// Where paths meet, what the path whose state of "sym" is "path"
  /// still holds of it, for that path to drop at its end: none unless
  /// "sym" is one of "owners" and the paths disagree about it
  /// ("joined" holds it on only some). "at" is where, for an error.
  static auto OwnedOnJoinPath(
    VariableSymbol const &sym, memory_state::MemoryState const &joined, memory_state::MemoryState const &path,
    Map<VariableSymbol const*, Scope const*> const &owners, Ast const &at,
    ScopeManager &sm, CompilerMetaData *meta) -> Opt<OwnedValue>;


  /// Raise if a partial move out of "sym" (in "state") stranded a
  /// value with a destructor of its own: taking something from
  /// inside a value leaves that value unable to be destroyed. Every
  /// place a move reached through is asked, not only "sym":
  /// "o.inner.val" strands "o.inner" even when "o" has no "drop".
  /// The places in "taken_apart" at or under "prefix" (field-name
  /// steps), as paths from it: the empty path is "prefix" itself.
  static auto TakenApartPathsUnder(
    Vec<regions::PlaceAt> const &taken_apart, Vec<regions::PlaceStep> const &prefix)
    -> Vec<Vec<spp::utils::InternedId>>;

  static auto CheckDestructorStillReachable(
    VariableSymbol const &sym, memory_state::MemoryState const &state, Ast const &exit_point,
    ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// Whether "sym" is "self" in its own type's "drop", whose end
  /// drops what is left of it field by field.
  static auto IsSelfInItsOwnDrop(
    VariableSymbol const &sym, TypeRef const &ref, ScopeManager &sm, CompilerMetaData *meta) -> bool;

  /// The parts "moves" took from inside the place "prefix" leads to
  /// (an empty one: the whole value), each as the field names walked
  /// from there. None when one went through a method's result (an
  /// opaque step), which names no field: what it came out of is not
  /// dropped at all. Todo: drop the rest of it.
  static auto MovedPathsUnder(
    Vec<regions::PlaceAt> const &moves, Vec<regions::PlaceStep> const &prefix)
    -> Opt<Vec<Vec<spp::utils::InternedId>>>;

  /// Find an overload for a types destructor, rather than
  /// just the prototype; instantiating it needs the block
  /// that declares it and the arguments that block was bound
  /// with too.
  static auto FindDropOverloadInfo(
    TypeSymbol const &type_sym, ScopeManager &sm,
    CompilerMetaData *meta) -> FnOverload;

  /// The prototype a destructor call is actually made against,
  /// which for a destructor declared in a generic "sup" block
  /// is the instantiation for that block's arguments rather
  /// than the template. The lookup and the minting are driven
  /// from the same scope (the type symbol's), so that both
  /// normalise the arguments identically.
  static auto DropProtoFor(
    TypeSymbol const &type_sym, ScopeManager &sm, CompilerMetaData *meta,
    const bool instantiate) -> FunctionPrototypeAst*;
}

auto spp::analyse::utils::drop_utils::FindDropOverloadInfo(
  TypeSymbol const &type_sym, ScopeManager &sm,
  CompilerMetaData *meta) -> FnOverload {

  // The "none" mini-constructor for the function overload
  // type, setting every field to nullptr.
  const auto none = [] {
    return FnOverload{
      .FnScope = nullptr,
      .Proto = nullptr,
      .ReachedThrough = nullptr,
      .FwdType = nullptr
    };
  };

  // A bound generic parameter stands for its argument, and it
  // is the argument that has the methods.
  if (const auto bound = type_sym.AsBound(); bound != &type_sym) {
    return FindDropOverloadInfo(*bound, sm, meta);
  }

  // A generic that was never bound, or a symbol with no
  // scope of its own, has no attributes and no methods
  // to find. Todo: What if we constrain generic with Drop?
  if (type_sym.LinkedScope == nullptr) { return none(); }

  // The type only has a destructor if it superimposes
  // "Drop". This is checked before looking for the method,
  // because a class is free to declare a method called
  // "drop" without meaning this at all.
  const auto superimposes_drop = genex::any_of(
    type_sym.LinkedScope->GetSupScopes(), [&](auto const *sup_scope) {
      if (sup_scope->LinkedTypeSymbol == nullptr) { return false; }
      return marker_sups::IsDropMarker(TypeRef::ForKindCheck(*sup_scope), *sup_scope);
    });
  if (not superimposes_drop) { return none(); }

  // Find the "drop" the type actually inherits.
  // "GetAllFnScopes" searches the sup scopes,
  // so an override on the type itself and an
  // implementation inherited from a type it extends
  // are both found here.
  const auto drop_name = IdentifierAst(0, "drop");
  auto overloads = overload_resolution::GetAllFnScopes(
    drop_name, type_sym.LinkedScope, sm, meta);

  for (auto &overload : overloads) {
    // "Drop::drop" itself is abstract with an empty body:
    // a type that superimposes "Drop" but never overrides
    // "drop" resolves to it, and calling it would be a call
    // into nothing.
    if (overload.Proto->AbstractAnnotation != nullptr) { continue; }

    // Destroying a value consumes it, so the destructor is
    // the "self" overload taking nothing else. Anything with
    // a borrow convention, or with other parameters, is an
    // unrelated method that happens to share the name.
    const auto self_param = overload.Proto->FnParamGroup->GetSelfParam();
    if (self_param == nullptr or self_param->Conv != nullptr) { continue; }
    if (not overload.Proto->FnParamGroup->GetNonSelfParams().empty()) { continue; }
    return std::move(overload);
  }

  return none();
}

auto spp::analyse::utils::drop_utils::DropProtoFor(
  TypeSymbol const &type_sym, ScopeManager &sm, CompilerMetaData *meta,
  const bool instantiate) -> FunctionPrototypeAst* {
  // Get the type symbol and find the drop overload. A nullptr
  // overload means that std::mem::ops::drop will be used (ie
  // consume all and drop in reverse attribute order). Todo
  // verify this about nullptr overload proto retrieved.
  auto const &sym = *type_sym.AsBound();
  const auto overload = FindDropOverloadInfo(sym, sm, meta);
  if (overload.Proto == nullptr or sym.LinkedScope == nullptr) { return overload.Proto; }

  // Either just find the overload or instantiate it too (this
  // will only instantiate it if it hasn't already been).
  auto tm = sm.NewManagerAt(sym.LinkedScope);
  const auto sup_gns = overload.ReachedThrough != nullptr
    ? overload.ReachedThrough->GetGns()
    : Vec<Unique<GenericArgumentAst>>();
  const auto args_id = tm.CurrentScope->ArgsIdOf(
    sup_gns | genex::views::transform([](auto const &a) { return a.get(); }) | genex::to<Vec>(), nullptr);
  return instantiate
    ? monomorphization::RequireFnInstance(overload.Proto, overload.FnScope, args_id, &tm, meta)
    : monomorphization::FindRequiredFnInstance(overload.Proto, args_id);
}

auto spp::analyse::utils::drop_utils::FindDropOverload(
  TypeSymbol const &type_sym, ScopeManager &sm,
  CompilerMetaData *meta) -> FunctionPrototypeAst* {
  // Wrap the implementation, specifying no instantiation.
  return DropProtoFor(type_sym, sm, meta, false);
}

auto spp::analyse::utils::drop_utils::EndsArgument(
  FunctionPrototypeAst const &proto, FunctionCallArgumentAst const &arg, const bool is_receiver,
  ScopeManager &sm, CompilerMetaData *meta) -> bool {
  // Only a value given as it is can be ended, not a borrow of one.
  if (arg.Conv != nullptr) { return false; }
  if (auto const *const builtin = proto.BuiltinAnnotation; builtin != nullptr) {
    const auto name = builtin->FnArgGroup->At("name")->Val->As<StringLiteralAst>()->CppVal();
    if (name == "std.mem.ops.drop") { return true; }
  }
  if (not is_receiver) { return false; }
  const auto receiver = arg.Val->InferTypeRef(&sm, meta);
  if (receiver.Symbol == nullptr or receiver.IsBorrowed()) { return false; }
  const auto own_drop = FindDropOverload(*receiver.Symbol, sm, meta);
  return own_drop != nullptr and (own_drop == &proto
    or (own_drop->BuiltinAnnotation != nullptr and own_drop->BuiltinAnnotation == proto.BuiltinAnnotation));
}

auto spp::analyse::utils::drop_utils::NeedsDrop(
  TypeRef const &type, ScopeManager &sm,
  CompilerMetaData *meta) -> bool {
  using type_members::GetAllParts;
  using type_predicates::IsTypeGenerator;

  // A borrow does not own what it points at, so nothing
  // behind it is this scope's to destroy.
  if (type.Symbol == nullptr or type.IsBorrowed()) { return false; }

  // Everything below - copyability, the sup chain, the
  // attributes - is a property of the type actually being
  // destroyed rather than of the name it arrived under,
  // so a bound parameter is read through first. An unbound
  // parameter has no linked scope, and nothing to drop.
  auto const &type_sym = *type.Symbol->AsBound();
  if (type_sym.LinkedScope == nullptr) { return false; }

  // Generators are managed by the handle's owner (special
  // management).
  if (IsTypeGenerator(type, *sm.CurrentScope)) { return true; }

  // A copyable value does not need dropping because it
  // can't ever be moved, so "dropping" it is meaningless.
  if (type_sym.IsCopyable()) { return false; }

  // A destructor of its own settles it without having to
  // look at the attributes at all
  if (FindDropOverloadInfo(type_sym, sm, meta).Proto != nullptr) { return true; }

  // A variant holds one of its members at a time, behind a
  // tag rather than as parts: it is worth dropping if any
  // member it can hold is.
  if (type_predicates::IsTypeVariant(type, *sm.CurrentScope)) {
    return genex::any_of(
      type_members::VariantMemberRefs(type, *sm.CurrentScope), [&](auto const &member) {
        return NeedsDrop(member, sm, meta);
      });
  }

  // Otherwise the type is only worth dropping if something
  // it holds is, each part read where it was found. A type
  // cannot contain itself by value, so the recursion is
  // bounded by the nesting depth of the type.
  return genex::any_of(
    GetAllParts(type, *sm.CurrentScope, true), [&](auto const &part) {
      return part.Ref.Symbol != &type_sym and NeedsDrop(part.Ref, sm, meta);
    });
}

auto spp::analyse::utils::drop_utils::EnsureDropInstantiated(
  TypeRef const &type, ScopeManager &sm, CompilerMetaData *meta,
  Vec<Vec<spp::utils::InternedId>> const &taken_apart) -> void {
  //
  auto seen = Set<TypeSymbol const*>();

  // Todo: can we use c++23/26 explicit "self" here?
  // MSVC does not see block-scope using-declarations from inside
  // this generic lambda, so the calls below are qualified.
  const auto walk = [&](auto const &self, TypeRef const &ref) -> void {
    // A bound generic parameter stands for its argument, so
    // everything below is a property of the type actually being
    // destroyed rather than of the name it arrived under.
    if (ref.Symbol == nullptr or ref.IsBorrowed()) { return; }
    auto const &sym = *ref.Symbol->AsBound();
    if (sym.LinkedScope == nullptr) { return; }
    if (not seen.insert(&sym).second) { return; }

    // A generator is destroyed by "llvm.coro.destroy", which
    // calls nothing of ours, and anything that destroys to
    // nothing needs nothing minted for it.
    if (type_predicates::IsTypeGenerator(ref, *sm.CurrentScope)) { return; }
    if (not NeedsDrop(ref, sm, meta)) { return; }

    // A destructor of its own is the whole of this type's
    // destruction: "EmitDrop" calls it and does not go on to the
    // attributes, because the destructor is what answers for them.
    if (DropProtoFor(sym, sm, meta, true) != nullptr) { return; }

    // A variant is destroyed as whichever member it holds, so
    // every member's destructor has to exist.
    if (type_predicates::IsTypeVariant(ref, *sm.CurrentScope)) {
      for (auto const &member : type_members::VariantMemberRefs(ref, *sm.CurrentScope)) { self(self, member); }
      return;
    }

    // Otherwise destruction is part by part, and it is their
    // destructors that have to exist.
    for (auto const &part : type_members::GetAllParts(ref, *sm.CurrentScope, true)) {
      if (part.Ref.Symbol == &sym) { continue; }
      self(self, part.Ref);
    }
  };

  // Recursive drop search. A place taken apart is destroyed part by
  // part, its own destructor not run, so its parts are searched
  // (and, a part further in being taken apart too, theirs).
  const auto apart = [&](auto const &self, TypeRef const &ref, Vec<spp::utils::InternedId> &path) -> void {
    if (not genex::contains(taken_apart, path)) { walk(walk, ref); return; }
    for (auto const &part : type_members::GetAllParts(ref, *sm.CurrentScope, true)) {
      path.emplace_back(part.Step->NameId());
      self(self, part.Ref, path);
      path.pop_back();
    }
  };
  auto path = Vec<spp::utils::InternedId>();
  apart(apart, type, path);
}

auto spp::analyse::utils::drop_utils::CheckDestructorStillReachable(
  VariableSymbol const &sym, memory_state::MemoryState const &state, Ast const &exit_point,
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  // Nothing taken out of it is nothing to put back.
  if (state.PartialMoves.empty()) { return; }
  if (sym.Type == nullptr) { Ice(exit_point, *sm.CurrentScope, "partly moved variable has no type"); }

  // A borrow does not own what it points at, so the value
  // behind it is not this scope's to destroy.
  if (sym.IsBorrow()) { return; }

  const auto root = TypeRef::Of(*sym.Type, *sm.CurrentScope);
  const auto taken_apart = TakenApartPathsUnder(state.TakenApart, {});
  for (auto const &move : state.PartialMoves) {
    RaiseIfPathStrandsDestructor(root, move.Of->Steps, *move.Site, exit_point, sm, meta, taken_apart);
  }
}

auto spp::analyse::utils::drop_utils::TakenApartPathsUnder(
  Vec<regions::PlaceAt> const &taken_apart, Vec<regions::PlaceStep> const &prefix)
  -> Vec<Vec<spp::utils::InternedId>> {
  auto paths = Vec<Vec<spp::utils::InternedId>>();
  for (auto const &place : taken_apart) {
    auto const &steps = place.Of->Steps;
    if (steps.size() < prefix.size() or not std::equal(prefix.begin(), prefix.end(), steps.begin())) { continue; }
    auto &path = paths.emplace_back();
    for (auto i = prefix.size(); i < steps.size(); ++i) { path.emplace_back(steps[i].Name); }
  }
  return paths;
}

auto spp::analyse::utils::drop_utils::RaiseIfPathStrandsDestructor(
  TypeRef const &root, Vec<regions::PlaceStep> const &steps, Ast const &move,
  Ast const &exit_point, ScopeManager &sm, CompilerMetaData *meta,
  Vec<Vec<spp::utils::InternedId>> const &taken_apart) -> void {
  // Each value the path reaches *through* is asked in turn: the root first, then one step further in for each name
  // the path passes on its way. The place it lands on is not one of them - taking a whole field out hands that
  // field's destructor to whoever received it, and only taking something from inside a value strands the value's
  // own. That is what stops the last step being walked, and what makes "let x = o.inner" fine where
  // "let x = o.inner.val" is not.
  for (auto i = 0uz; i < steps.size(); ++i) {
    const auto [part_ref, part_scope] = regions::DescendToPart(root, *sm.CurrentScope, steps, i);
    if (part_ref.Symbol == nullptr or part_scope == nullptr) { break; }
    const auto apart = genex::any_of(taken_apart, [&](auto const &path) {
      return path.size() == i and std::equal(path.begin(), path.end(), steps.begin(), [](auto id, auto const &step) {
        return id == step.Name;
      });
    });
    if (apart) { continue; }
    const auto destructor = FindDropOverload(*part_ref.Symbol, sm, meta);
    if (destructor == nullptr) { continue; }

    // Held in a local so the view handed to the error outlives it.
    const auto owner_name = part_ref.Symbol->FqName()->WithoutGns()->ToString();
    Raise<errors::SppPartialMoveOfDestructibleValueError>(
      ERR_ARGS(exit_point, move, *destructor->Name, StrView(owner_name)));
  }
}

auto spp::analyse::utils::drop_utils::IsOwnedIn(
  VariableSymbol const &sym, memory_state::MemoryState const &state, ScopeManager const &sm) -> bool {
  // Only a local owns a value it has to answer for. A generic or a constant has no runtime existence, and a
  // flow-narrowing symbol is a view of another symbol's value (which owns it). A "$" temporary a desugaring made is a
  // local like any other. So is a capture taken by value, in the closure's body: such a closure is a "FunMov", so a
  // call consumes it, and its body owns what it captured as a function's owns its parameters, dropping what it did
  // not move out (the caller then only frees the environment). A borrowed capture owns nothing ("IsBorrow" below).
  if (sym.Kind != VariableKind::Local and sym.Kind != VariableKind::Temporary and sym.Kind != VariableKind::Capture) {
    return false;
  }

  // A closed pack holds nothing itself: its elements are parameters of their own, each answering for its value.
  // An open one holds what its elements are: nothing owned when each is copied ("..xs: S32", "..xs: Ts" with
  // "Ts: Copy"), whatever its tuple of unknown length says.
  if (sym.Pack.has_value() and not sym.Pack->Open) { return false; }
  if (sym.Pack.has_value()) {
    const auto element = sm.CurrentScope->FindTypeSymbol(
      sym.Pack->Element->WithoutConvention().get());
    if (element != nullptr and element->IsCopyable()) { return false; }
  }
  if (sym.Type == nullptr or sym.MemInfo == nullptr) {
    Ice(*sym.Name, *sm.CurrentScope, "local variable has no type");
  }

  // A borrow points at a value that belongs to someone else; a copyable value is never owned by one place, as
  // copying leaves the original in place; and a "!" value never exists, the code binding it cannot finish.
  if (sym.IsBorrow()) { return false; }
  const auto type = TypeRef::Of(*sym.Type, *sm.CurrentScope);
  if (not type.IsMoveOnly()) { return false; }


  // Taken on some of the paths that met and not on others, whole or in part: a path that did not take a part
  // still owns it. The witnesses kept are the first path's, so the value can read as taken while another path
  // still owns it.
  const auto owned_on_some_path = state.MaybeInit.has_value() or not state.MaybePartialMoves.empty();

  // The value already left, whole.
  if (state.AstInitialization == nullptr and not owned_on_some_path) { return false; }

  // A holder of loans (a "&mut"-capturing closure, a borrowing generator or future) is owned like any other value:
  // it owns its environment or frame. It cannot be moved away, but a drop ends it and its loans together.

  // Taking every non-copyable part off a value leaves nothing to own. A destructure is the usual way that happens,
  // and a case pattern's destructure only ever records the parts it bound, never the value itself, so the parts are
  // all there is to go on.
  if (state.PartialMoves.empty() or owned_on_some_path) { return true; }
  return not regions::RegionConsumed(
    regions::Place{.Root = const_cast<VariableSymbol*>(&sym), .Steps = {}}, type, *sm.CurrentScope,
    state.PartialMoves);
}

auto spp::analyse::utils::drop_utils::OwnedOnPath(
  VariableSymbol const &sym, memory_state::MemoryState const &state, Scope const &owner_scope,
  Ast const &at, ScopeManager &sm, CompilerMetaData *meta) -> Opt<OwnedValue> {
  if (not IsOwnedIn(sym, state, sm)) { return Nullopt; }

  // Held on only some of the paths that met before this one: each path that still held it dropped it before they met.
  if (state.MaybeInit.has_value() or not state.MaybePartialMoves.empty()) { return Nullopt; }

  auto moved = MovedPathsUnder(state.PartialMoves, {});
  if (not moved.has_value()) { return Nullopt; }

  // What a destructure took apart is dropped field by field, its type's own "drop" not run, and so is "self" left at
  // the end of its own type's "drop" (running that "drop" again would recurse): a part moved out of either strands
  // nothing. Any other value's own "drop" cannot be run on what is left of it with a part missing (E106).
  const auto ref = sym.TypeRefIn(owner_scope);
  auto taken_apart = TakenApartPathsUnder(state.TakenApart, {});
  if (IsSelfInItsOwnDrop(sym, ref, sm, meta)) { taken_apart.emplace_back(); }
  else { CheckDestructorStillReachable(sym, state, at, sm, meta); }

  // Nothing written calls this drop, so its destructor may never have been instantiated, and destroying it without
  // one would go field by field: a "Vec" frees its buffer without dropping the elements in it.
  EnsureDropInstantiated(ref, sm, meta, taken_apart);
  return OwnedValue{
    .Name = sym.Name, .Moved = std::move(*moved), .DeclaredAt = sym.Name->PosStart(),
    .TakenApart = std::move(taken_apart)};
}

auto spp::analyse::utils::drop_utils::EndRegion(
  Scope const &scope, Ast const &at, ScopeManager &sm,
  CompilerMetaData *meta) -> Vec<OwnedValue> {
  auto owned = Vec<OwnedValue>();
  for (auto *const sym : scope.GetOwners()) {
    // A part taken from inside a value with its own "drop" strands it (E106), even when the part was all it owned:
    // only a whole destructure in the type's own methods takes such a value apart.
    if (sym->Type != nullptr and not IsSelfInItsOwnDrop(*sym, sym->TypeRefIn(scope), sm, meta)) {
      CheckDestructorStillReachable(*sym, memory_state::StateOf(*sm.CurrentScope, *sym), at, sm, meta);
    }
    if (auto value = OwnedOnPath(*sym, memory_state::StateOf(*sm.CurrentScope, *sym), scope, at, sm, meta)) {
      owned.emplace_back(std::move(*value));
    }
  }
  SortNewestFirst(owned);
  return owned;
}

auto spp::analyse::utils::drop_utils::SortNewestFirst(
  Vec<OwnedValue> &owned) -> void {
  // As a type drops its fields: by where each is declared, a table having no order of its own.
  genex::actions::sort(owned, std::greater{}, [](auto const &value) { return value.DeclaredAt.value_or(0uz); });
}

auto spp::analyse::utils::drop_utils::OwnersAround(
  Scope const &scope, CompilerMetaData const *meta) -> Map<VariableSymbol const*, Scope const*> {
  auto owners = Map<VariableSymbol const*, Scope const*>();
  if (meta == nullptr or meta->EnclosingFnScope == nullptr or &scope == meta->EnclosingFnScope) { return owners; }
  for (auto const *s = scope.Parent; s != nullptr; s = s->Parent) {
    for (auto *const sym : s->GetOwners()) { owners.emplace(sym, s); }
    if (s == meta->EnclosingFnScope) { break; }
  }
  return owners;
}

auto spp::analyse::utils::drop_utils::OwnedOnJoinPath(
  VariableSymbol const &sym, memory_state::MemoryState const &joined, memory_state::MemoryState const &path,
  Map<VariableSymbol const*, Scope const*> const &owners, Ast const &at,
  ScopeManager &sm, CompilerMetaData *meta) -> Opt<OwnedValue> {
  if (not joined.MaybeInit.has_value() and joined.MaybePartialMoves.empty()) { return Nullopt; }
  const auto owner = owners.find(&sym);
  if (owner == owners.end()) { return Nullopt; }
  return OwnedOnPath(sym, path, *owner->second, at, sm, meta);
}

auto spp::analyse::utils::drop_utils::ReleaseOwnedLoans(
  Scope const &scope) -> void {
  // What matters is where the holder was declared, not where
  // the borrow was made: a handle declared further out carries
  // its borrow on past this scope.
  auto &loans = memory_state::LoansOf(scope);
  for (auto const *const sym : scope.GetOwners()) { memory_state::ReleaseLoans(loans, *sym); }
}

auto spp::analyse::utils::drop_utils::JoinPaths(
  Vec<VariableSymbol*> const &syms, Vec<JoinPath> const &paths,
  Map<VariableSymbol const*, Scope const*> const &owners, ScopeManager &sm, CompilerMetaData *meta,
  Opt<JoinPath> const &carried) -> Vec<Vec<OwnedValue>> {
  auto drops = Vec<Vec<OwnedValue>>(paths.size());
  for (auto *const sym : syms) {
    const auto state_on = [&](JoinPath const &path) {
      return path.State == nullptr
        ? memory_state::StateOf(*sm.CurrentScope, *sym)
        : memory_state::StateIn(*path.State, *sym);
    };
    auto on_paths = paths | genex::views::transform(state_on) | genex::to<Vec>();
    auto states = Vec<Pair<memory_state::MemoryState, Ast*>>();
    if (carried.has_value()) { states.emplace_back(state_on(*carried), carried->At); }
    for (auto i = 0uz; i < paths.size(); ++i) { states.emplace_back(on_paths[i], paths[i].At); }
    auto joined = memory_state::JoinAll(std::move(states), *sym, *sm.CurrentScope);
    for (auto i = 0uz; i < paths.size(); ++i) {
      if (auto value = OwnedOnJoinPath(*sym, joined, on_paths[i], owners, *paths[i].End, sm, meta)) {
        drops[i].emplace_back(std::move(*value));
      }
    }
    memory_state::StateOf(*sm.CurrentScope, *sym) = std::move(joined);
  }
  for (auto &path_drops : drops) { SortNewestFirst(path_drops); }
  return drops;
}

SPP_MOD_BEGIN
spp::analyse::utils::drop_utils::TemporaryRegion::TemporaryRegion(
  Scope const &scope) :
  _Scope(&scope),
  _From(scope.Declarations.size()) {
}

auto spp::analyse::utils::drop_utils::TemporaryRegion::Whole(
  Scope const &scope) -> TemporaryRegion {
  auto region = TemporaryRegion(scope);
  region._From = 0;
  return region;
}

auto spp::analyse::utils::drop_utils::TemporaryRegion::Declared() const -> Vec<VariableSymbol*> {
  auto declared = Vec<VariableSymbol*>();
  for (auto *const sym : _Scope->Declarations | genex::views::drop(_From)) {
    if (sym->Kind == VariableKind::Temporary and not genex::contains(declared, sym)) { declared.emplace_back(sym); }
  }
  return declared;
}

auto spp::analyse::utils::drop_utils::TemporaryRegion::End(
  Ast const &end, ScopeManager &sm, CompilerMetaData *meta, IdentifierAst const *const own_let) const
  -> Vec<OwnedValue> {
  auto owned = Vec<OwnedValue>();
  for (auto *const sym : Declared()) {
    if (own_let != nullptr and sym->Name->NameId() == own_let->NameId()) { continue; }

    // Dropped here, the temporary is gone: the scope's end does not
    // drop it again (one a destructure took apart is still owned).
    auto &state = memory_state::StateOf(*sm.CurrentScope, *sym);
    if (auto value = OwnedOnPath(*sym, state, *_Scope, end, sm, meta)) {
      owned.emplace_back(std::move(*value));
      state.MovedBy(end);
    }
  }
  SortNewestFirst(owned);
  return owned;
}

auto spp::analyse::utils::drop_utils::TemporaryRegion::Held(
  Ast const &end, ScopeManager &sm, CompilerMetaData *meta, Vec<VariableSymbol*> const &except) const
  -> Vec<OwnedValue> {
  auto owned = Vec<OwnedValue>();
  for (auto *const sym : Declared()) {
    if (genex::contains(except, sym)) { continue; }
    if (auto value = OwnedOnPath(*sym, memory_state::StateOf(*sm.CurrentScope, *sym), *_Scope, end, sm, meta)) {
      owned.emplace_back(std::move(*value));
    }
  }
  SortNewestFirst(owned);
  return owned;
}
SPP_MOD_END

auto spp::analyse::utils::drop_utils::ForgetIterationBindings(
  Scope const &loop_scope) -> void {
  // A state no longer in the flow is read from the symbol again,
  // as Stage 7 left it: as the first iteration reads it.
  auto *const flow = memory_state::FlowOf(loop_scope);
  if (flow == nullptr) { return; }
  for (auto const &sym : loop_scope.LetBindings) { flow->States.erase(sym.get()); }
}

auto spp::analyse::utils::drop_utils::OwnedUpToFn(
  Ast const &exit_point,
  ScopeManager &sm, CompilerMetaData *meta) -> OwnedScopes {
  // Outside a function there is nothing to drop: a module-level
  // constant outlives every scope that reads it. Every scope out
  // to the function's is left: an exit out of more loops than
  // there are.
  if (meta->EnclosingFnScope == nullptr) { return {}; }
  auto iteration = Vec<OwnedValue>();
  return OwnedUpToLoop(exit_point, std::numeric_limits<std::size_t>::max(), false, sm, meta, iteration);
}

auto spp::analyse::utils::drop_utils::OwnedAtSuspend(
  Ast const &suspend_point, ScopeManager &sm,
  CompilerMetaData *meta) -> OwnedScopes {
  // Destroying the frame ends every borrow there is: what holds one in the frame goes with it, and the caller gave up
  // whatever was lent out to it (a yield) before the frame could be destroyed. Otherwise it leaves every scope out to
  // the coroutine's as a "ret" would, on a state put back afterwards: the path goes on past the suspension untouched.
  const auto saved = memory_state::SnapshotFlow(*sm.CurrentScope);
  memory_state::LoansOf(*sm.CurrentScope).clear();
  auto owned = OwnedUpToFn(suspend_point, sm, meta);
  memory_state::RestoreSnapshot(saved);
  return owned;
}

auto spp::analyse::utils::drop_utils::OwnedUpToLoop(
  Ast const &exit_point, const std::size_t num_exits,
  const bool has_skip, ScopeManager &sm, CompilerMetaData *meta,
  Vec<OwnedValue> &iteration) -> OwnedScopes {
  // Walked on a copy of the state, put back afterwards: stage 8
  // walks statements in order rather than following branches.
  auto loops_seen = 0uz;
  auto owned = OwnedScopes();
  const auto saved = memory_state::SnapshotFlow(*sm.CurrentScope);

  for (auto const *scope = sm.CurrentScope; scope != nullptr; scope = scope->Parent) {
    // An iterable loop is rewritten into a conditional one
    // before this stage, so matching the conditional form
    // covers both.
    const auto is_loop = scope->AstNode != nullptr
      and asts::AstAs<LoopConditionalExpressionAst>(scope->AstNode) != nullptr;

    if (is_loop) {
      // The loop after the ones being exited is the one a
      // trailing "skip" continues, so its own scope survives
      // this statement, but the iteration it is in ends.
      loops_seen += 1;
      if (loops_seen > num_exits) {
        if (has_skip) {
          iteration = TemporaryRegion::Whole(*scope).End(exit_point, sm, meta);
        }
        break;
      }
    }

    owned.emplace_back(EndRegion(*scope, exit_point, sm, meta));
    if (is_loop and loops_seen == num_exits and not has_skip) { break; }
    if (scope == meta->EnclosingFnScope) { break; }
  }

  memory_state::RestoreSnapshot(saved);
  return owned;
}

auto spp::analyse::utils::drop_utils::OwnedBeforeWrite(
  VariableSymbol const *sym, ExpressionAst &target, ScopeManager &sm,
  CompilerMetaData *meta) -> Opt<OwnedValue> {
  // A "Copy" value is never dropped.
  const auto ref = target.InferTypeRef(&sm, meta).WithoutConvention();
  if (not ref.IsMoveOnly()) { return Nullopt; }

  // An abstract type ("Self" in a default method: "self@ = source.clone()" in "Clone::clone_from") says nothing about
  // what is dropped until a class is chosen; each class that inherits the method gets a copy with "Self" pinned, which
  // is checked here like any other body.
  auto const *const ref_scope = ref.Symbol->LinkedScope;
  if (ref_scope != nullptr and not type_members::GetUnimplementedAbstractMethods(*ref_scope).empty()) {
    return Nullopt;
  }

  // Through a borrow ("self.a = .." with "&mut self", "x@ = .."), the value written over belongs to whoever is
  // borrowed from, and nothing can have been taken out of it: it is there, whole.
  if (sym == nullptr or sym->IsBorrow()) {
    EnsureDropInstantiated(ref, sm, meta);
    return OwnedValue{};
  }

  // Of a value this scope owns, only a part still standing is dropped, less what was taken out of it.
  auto const &state = memory_state::StateOf(*sm.CurrentScope, *sym);
  if (sym->Kind != scopes::VariableKind::Local or state.AstInitialization == nullptr) { return Nullopt; }
  if (state.MaybeInit.has_value() or not state.MaybePartialMoves.empty()) { return Nullopt; }
  const auto place = regions::PlaceOf(target, *sm.CurrentScope);
  if (not place.has_value()) { return Nullopt; }
  if (regions::RegionConsumed(*place, ref, *sm.CurrentScope, state.PartialMoves)) { return Nullopt; }
  // What a destructure took apart in it is dropped field by field.
  auto moved = MovedPathsUnder(state.PartialMoves, place->Steps);
  if (not moved.has_value()) { return Nullopt; }
  auto taken_apart = TakenApartPathsUnder(state.TakenApart, place->Steps);
  EnsureDropInstantiated(ref, sm, meta, taken_apart);
  return OwnedValue{.Moved = std::move(*moved), .TakenApart = std::move(taken_apart)};
}

auto spp::analyse::utils::drop_utils::FieldReadOffTemporary(
  ExpressionAst &expr, ScopeManager &sm, CompilerMetaData *meta)
  -> asts::PostfixExpressionOperatorRuntimeMemberAccessAst* {
  using asts::PostfixExpressionAst;
  using asts::PostfixExpressionOperatorRuntimeMemberAccessAst;
  auto *const postfix = expr.To<PostfixExpressionAst>();
  auto *const field = postfix != nullptr ? postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() : nullptr;
  if (field == nullptr) { return nullptr; }
  auto *first = postfix;
  while (auto *const inner = first->Lhs->To<PostfixExpressionAst>()) {
    if (inner->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { break; }
    first = inner;
  }
  return IsTemporary(*first->Lhs, sm, meta) ? field : nullptr;
}

auto spp::analyse::utils::drop_utils::IsTemporary(
  ExpressionAst &expr, ScopeManager &sm,
  CompilerMetaData *meta) -> bool {
  // A value naming a place belongs to that place; a borrow
  // belongs to whatever it borrows; a value made at compile
  // time never exists at run time.
  // A call is never a place for this: "PlaceOf" derives a method call on a place from its receiver ("v.clone()" is
  // "v", one opaque step in), which is what a borrow the call hands back is reached through. An owned result is a
  // new value all the same, and only the borrow test below lets the call's result go.
  if (meta != nullptr and meta->InCompTimeValue) { return false; }
  const auto is_call = asts::IsFuncCall(&expr);
  if (not is_call and regions::PlaceOf(expr, *sm.CurrentScope).has_value()) { return false; }
  const auto ref = expr.InferTypeRef(&sm, meta);
  if (not ref.IsMoveOnly()) { return false; }

  // Dropped where nothing written calls its "drop", so it is instantiated here.
  EnsureDropInstantiated(ref, sm, meta);
  return true;
}

auto spp::analyse::utils::drop_utils::MovedPathsUnder(
  Vec<regions::PlaceAt> const &moves, Vec<regions::PlaceStep> const &prefix)
  -> Opt<Vec<Vec<spp::utils::InternedId>>> {
  auto moved = Vec<Vec<spp::utils::InternedId>>();
  for (auto const &move : moves) {
    auto const &steps = move.Of->Steps;
    if (steps.size() <= prefix.size() and not prefix.empty()) { continue; }
    if (not std::equal(prefix.begin(), prefix.end(), steps.begin())) { continue; }
    auto &path = moved.emplace_back();
    for (auto i = prefix.size(); i < steps.size(); ++i) {
      if (steps[i].Opaque) { return Nullopt; }
      path.emplace_back(steps[i].Name);
    }
  }
  return moved;
}

auto spp::analyse::utils::drop_utils::IsSelfInItsOwnDrop(
  VariableSymbol const &sym, TypeRef const &ref, ScopeManager &sm, CompilerMetaData *meta) -> bool {
  if (meta == nullptr or meta->EnclosingFnScope == nullptr or sym.Name->Val != "self" or ref.Symbol == nullptr) {
    return false;
  }
  auto const *const fn = asts::AstAs<FunctionPrototypeAst>(meta->EnclosingFnScope->AstNode);
  return fn != nullptr and fn->Name->Val == "drop" and FindDropOverload(*ref.Symbol, sm, meta) != nullptr;
}
