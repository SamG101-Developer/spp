module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.drop_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import genex;

namespace spp::analyse::utils::drop_utils {
  namespace {

    using overload_resolution::FnOverload;

    /// Find an overload for a types destructor, rather than
    /// just the prototype; instantiating it needs the block
    /// that declares it and the arguments that block was bound
    /// with too.
    auto FindDropOverloadInfo(
      TypeSymbol const &type_sym, ScopeManager &sm,
      CompilerMetaData *meta) -> FnOverload {
      using generate::common_types_precompiled::DROP;

      // The "none" mini-constructor for the function overload
      // type, setting every field to nullptr.
      const auto none = [] {
        return FnOverload{
          .FnScope = nullptr,
          .Proto = nullptr,
          .SupGns = nullptr,
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
          return TypeRef::ForKindCheck(*sup_scope).IsA(*DROP, *sup_scope);
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
        if (not overload.Proto->FnParamGroup->GetNonSelfParams().IsEmpty()) { continue; }
        return std::move(overload);
      }

      return none();
    }

    /// The prototype a destructor call is actually made against,
    /// which for a destructor declared in a generic "sup" block
    /// is the instantiation for that block's arguments rather
    /// than the template. The lookup and the minting are driven
    /// from the same scope (the type symbol's), so that both
    /// normalise the arguments identically.
    auto DropProtoFor(
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
      auto tm = ScopeManager(sm.GlobalScope, sym.LinkedScope);
      return instantiate
        ? monomorphization::InstantiateOverload(
          overload.Proto, overload.FnScope, *overload.SupGns, &tm, meta)
        : monomorphization::FindInstantiatedOverload(
          overload.Proto, *overload.SupGns, &tm);
    }
  }
}

auto spp::analyse::utils::drop_utils::FindDropOverload(
  TypeSymbol const &type_sym, ScopeManager &sm,
  CompilerMetaData *meta) -> FunctionPrototypeAst* {
  // Wrap the implementation, specifying no instantiation.
  return DropProtoFor(type_sym, sm, meta, false);
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
  TypeRef const &type, ScopeManager &sm,
  CompilerMetaData *meta) -> void {
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

    // Otherwise destruction is part by part, and it is their
    // destructors that have to exist.
    for (auto const &part : type_members::GetAllParts(ref, *sm.CurrentScope, true)) {
      if (part.Ref.Symbol == &sym) { continue; }
      self(self, part.Ref);
    }
  };

  // Recursive drop search.
  walk(walk, type);
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
auto spp::analyse::utils::drop_utils::CheckDestructorStillReachable(
  VariableSymbol const &sym,
  Ast const &exit_point,
  ScopeManager &sm,
  meta::CompilerMetaData *const meta)
  -> void {
  // Nothing taken out of it is nothing to put back.
  if (sym.MemInfo->AstPartialMoves.IsEmpty()) { return; }
  if (sym.Type == nullptr) { return; }

  // A borrow does not own what it points at, so the value
  // behind it is not this scope's to destroy.
  if (spp::get<0>(sym.MemInfo->AstBorrowed) != nullptr) { return; }
  if (sym.Type->GetConvention() != nullptr) { return; }

  const auto root = TypeRef::Of(*sym.Type, *sm.CurrentScope);
  for (auto const *move : sym.MemInfo->AstPartialMoves) {
    // A move leaves a hole in every place it reached *through*, so each of those is asked in turn: the symbol's
    // own type first, then one step further in for each name the path passes on its way. The place the move
    // landed on is not one of them - taking a whole field out hands that field's destructor to whoever received
    // it, and only taking something from inside a value strands the value's own. That is what stops the last
    // step being walked, and what makes "let x = o.inner" fine where "let x = o.inner.val" is not.
    const auto path = regions::RegionPath(*move);

    for (auto i = 0uz; i + 1 < path.Len(); ++i) {
      const auto [part_ref, part_scope] = regions::DescendToPart(root, *sm.CurrentScope, path, i);
      if (part_ref.Symbol == nullptr or part_scope == nullptr) { break; }

      const auto destructor = drop_utils::FindDropOverload(*part_ref.Symbol, sm, meta);
      if (destructor == nullptr) { continue; }

      // Held in a local so the view handed to the error
      // outlives it. The destructor is shown from the "sup"
      // block declaring it, which can be in another file than
      // the move.
      const auto owner_name = part_ref.Symbol->FqName()->WithoutGns()->ToString();
      auto const *const destructor_scope = FindDropOverloadInfo(*part_ref.Symbol->AsBound(), sm, meta).FnScope;
      Raise<errors::SppPartialMoveOfDestructibleValueError>(
        {destructor_scope != nullptr ? destructor_scope : sm.CurrentScope, sm.CurrentScope, sm.CurrentScope},
        ERR_ARGS(exit_point, *move, *destructor->Name, StrView(owner_name)));
    }
  }
}
