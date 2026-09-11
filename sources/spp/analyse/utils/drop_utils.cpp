module;
#include <spp/macros.hpp>

module spp.analyse.utils.drop_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.overload_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import genex;

namespace spp::analyse::utils::drop_utils {
  namespace {

    /**
     * The whole overload record for a type's destructor, rather than just its prototype: instantiating it needs the
     * block that declares it and the arguments that block was bound with as well.
     * @param type_sym The symbol of the type being destroyed.
     * @param sm The scope manager, positioned anywhere the type resolves from.
     * @param meta Associated metadata.
     * @return The overload, or one with a null @c Proto if this type has no destructor of its own.
     */
    auto FindDropOverloadInfo(
      scopes::TypeSymbol const &type_sym,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *meta)
      -> func_utils::FunctionOverload {
      //
      using type_compare::TypeEq;
      using asts::generate::common_types_precompiled::DROP;

      const auto none = [] { return func_utils::FunctionOverload{nullptr, nullptr, nullptr, nullptr}; };

      // A bound generic parameter stands for its argument, and it
      // is the argument that has the methods.
      if (auto *const bound = type_sym.AsBoundSymbol(); bound != &type_sym) {
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
        type_sym.LinkedScope->SupScopes(), [&](auto const *sup_scope) {
          if (sup_scope->TySym == nullptr) { return false; }
          return TypeEq(*sup_scope->TySym->FqName(), *DROP, *sup_scope, *sm.CurrentScope);
        });
      if (not superimposes_drop) { return none(); }

      // Find the "drop" the type actually inherits.
      // "GetAllFunctionScopes" searches the sup scopes,
      // so an override on the type itself and an
      // implementation inherited from a type it extends
      // are both found here.
      const auto drop_name = asts::IdentifierAst(0, "drop");
      auto overloads = func_utils::GetAllFunctionScopes(
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

    /**
     * The prototype a destructor call is actually made against, which for a destructor declared in a generic @c sup
     * block is the instantiation for that block's arguments rather than the template. The lookup and the minting are
     * driven from the same scope - the type's own - so that both normalise the arguments identically.
     * @param type_sym The symbol of the type being destroyed.
     * @param sm The scope manager, positioned anywhere the type resolves from.
     * @param meta Associated metadata.
     * @param instantiate Whether to mint the instantiation when there is not one yet.
     * @return The prototype, or @c nullptr if this type has no destructor of its own (or has one that has not been
     * instantiated and @p instantiate is false).
     */
    auto DropProtoFor(
      scopes::TypeSymbol const &type_sym,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *meta,
      const bool instantiate)
      -> asts::FunctionPrototypeAst* {
      auto const &sym = *type_sym.AsBoundSymbol();
      const auto overload = FindDropOverloadInfo(sym, sm, meta);
      if (overload.Proto == nullptr or sym.LinkedScope == nullptr) { return overload.Proto; }

      auto tm = scopes::ScopeManager(sm.GlobalScope, sym.LinkedScope);
      return instantiate
        ? overload_utils::InstantiateOverload(
          overload.Proto, overload.FnScope, *overload.SupGenerics, &tm, meta)
        : overload_utils::FindInstantiatedOverload(
          overload.Proto, *overload.SupGenerics, &tm);
    }
  }
}

auto spp::analyse::utils::drop_utils::FindDropOverload(
  scopes::TypeSymbol const &type_sym,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> asts::FunctionPrototypeAst* {
  return DropProtoFor(type_sym, sm, meta, false);
}

auto spp::analyse::utils::drop_utils::NeedsDrop(
  scopes::TypeSymbol const &type_sym,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> bool {
  //
  using type_members::GetAllParts;
  using type_predicates::IsTypeGen;

  // A borrow does not own what it points at, so nothing
  // behind it is this scope's to destroy. Generators are
  // managed by the handles owner (special management).
  if (type_sym.Convention != nullptr) { return false; }
  if (type_sym.LinkedScope == nullptr) { return false; }

  // Everything below - copyability, the sup chain, the attributes - is a property of the type actually being
  // destroyed rather than of the name it arrived under, so resolve a bound parameter through first. An unbound
  // parameter has no linked scope and is handled by the check below.
  if (auto *const bound = type_sym.AsBoundSymbol(); bound != &type_sym) {
    return NeedsDrop(*bound, sm, meta);
  }
  if (IsTypeGen(*type_sym.FqName(), *sm.CurrentScope)) { return true; }

  // A copyable value does not need dropping because it
  // can't ever be moved, so "dropping" it is meaningless.
  if (type_sym.IsCopyable()) { return false; }

  // A destructor of its own settles it without having to
  // look at the attributes at all
  if (FindDropOverloadInfo(type_sym, sm, meta).Proto != nullptr) { return true; }

  // Otherwise the type is only worth dropping if something
  // it holds is. A type cannot contain itself by value, so
  // the recursion is bounded by the nesting depth of the
  // type.
  return genex::any_of(
    GetAllParts(*type_sym.FqName(), *sm.CurrentScope, true), [&](auto const &part) {
      return part.Sym != nullptr and part.Sym != &type_sym and NeedsDrop(*part.Sym, sm, meta);
    });
}

auto spp::analyse::utils::drop_utils::EnsureDropInstantiated(
  scopes::TypeSymbol const &type_sym,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> void {
  //
  auto seen = Set<scopes::TypeSymbol const*>();

  // Todo: can we use c++23/26 explicit "self" here?
  // MSVC does not see block-scope using-declarations from inside
  // this generic lambda, so the calls below are qualified.
  const auto walk = [&](auto const &self, scopes::TypeSymbol const &sym) -> void {
    if (sym.Convention != nullptr or sym.LinkedScope == nullptr) { return; }
    if (not seen.insert(&sym).second) { return; }

    // A bound generic parameter stands for its argument, so
    // everything below is a property of the type actually being
    // destroyed rather than of the name it arrived under.
    if (sym.IsGeneric and sym.LinkedScope->TySym != nullptr and sym.LinkedScope->TySym.get() != &sym) {
      self(self, *sym.LinkedScope->TySym);
      return;
    }

    // A generator is destroyed by "llvm.coro.destroy", which
    // calls nothing of ours, and anything that destroys to
    // nothing needs nothing minted for it.
    if (type_predicates::IsTypeGen(*sym.FqName(), *sm.CurrentScope)) { return; }
    if (not NeedsDrop(sym, sm, meta)) { return; }

    // A destructor of its own is the whole of this type's
    // destruction: "EmitDrop" calls it and does not go on to the
    // attributes, because the destructor is what answers for them.
    if (DropProtoFor(sym, sm, meta, true) != nullptr) { return; }

    // Otherwise destruction is part by part, and it is their
    // destructors that have to exist.
    for (auto const &part : type_members::GetAllParts(*sym.FqName(), *sm.CurrentScope, true)) {
      if (part.Sym == nullptr or part.Sym == &sym) { continue; }
      self(self, *part.Sym);
    }
  };

  // Recursive drop search.
  walk(walk, type_sym);
}
