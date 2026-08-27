module;
#include <spp/macros.hpp>

module spp.analyse.utils.drop_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
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

auto spp::analyse::utils::drop_utils::FindDropOverload(
  scopes::TypeSymbol const &type_sym,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> asts::FunctionPrototypeAst* {
  //
  using type_utils::TypeEq;
  using asts::generate::common_types_precompiled::DROP;

  // A bound generic parameter stands for its argument: the
  // symbol keeps the parameter's name ("T"), but the scope
  // it links to is the argument's.
  if (type_sym.IsGeneric and type_sym.LinkedScope != nullptr and type_sym.LinkedScope->TySym != nullptr
    and type_sym.LinkedScope->TySym.get() != &type_sym) {
    return FindDropOverload(*type_sym.LinkedScope->TySym, sm, meta);
  }

  // A generic that was never bound, or a symbol with no
  // scope of its own, has no attributes and no methods
  // to find. Todo: What if we constrain generic with Drop?
  if (type_sym.LinkedScope == nullptr) { return nullptr; }

  // The type only has a destructor if it superimposes
  // "Drop". This is checked before looking for the method,
  // because a class is free to declare a method called
  // "drop" without meaning this at all.
  const auto superimposes_drop = genex::any_of(
    type_sym.LinkedScope->SupScopes(), [&](auto const *sup_scope) {
      if (sup_scope->TySym == nullptr) { return false; }
      return TypeEq(*sup_scope->TySym->FqName(), *DROP, *sup_scope, *sm.CurrentScope);
    });
  if (not superimposes_drop) { return nullptr; }

  // Find the "drop" the type actually inherits.
  // "GetAllFunctionScopes" searches the sup scopes,
  // so an override on the type itself and an
  // implementation inherited from a type it extends
  // are both found here.
  const auto drop_name = asts::IdentifierAst(0, "drop");
  const auto overloads = func_utils::GetAllFunctionScopes(
    drop_name, type_sym.LinkedScope, sm, meta);

  for (auto const &overload : overloads) {
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
    return overload.Proto;
  }

  return nullptr;
}

auto spp::analyse::utils::drop_utils::NeedsDrop(
  scopes::TypeSymbol const &type_sym,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> bool {
  //
  using type_utils::GetAllAttrs;

  // A borrow does not own what it points at, so nothing
  // behind it is this scope's to destroy. Generators are
  // managed by the handles owner (special management).
  if (type_sym.Convention != nullptr) { return false; }
  if (type_sym.LinkedScope == nullptr) { return false; }

  // A bound generic parameter stands for its argument: the symbol keeps the parameter's name ("T"), but the scope it
  // links to is the argument's. Everything below - copyability, the sup chain, the attributes - is a property of the
  // type actually being destroyed rather than of the name it arrived under, so resolve through first. An unbound
  // parameter has no linked scope and is handled by the check below.
  if (type_sym.IsGeneric and type_sym.LinkedScope != nullptr and type_sym.LinkedScope->TySym != nullptr
    and type_sym.LinkedScope->TySym.get() != &type_sym) {
    return NeedsDrop(*type_sym.LinkedScope->TySym, sm, meta);
  }
  if (type_utils::IsTypeGen(*type_sym.FqName(), *sm.CurrentScope)) { return true; }

  // A copyable value owns nothing that has to be released: copying leaves the original in place, so there was never
  // a single owner to answer for it. Checked before the overload lookup, which now walks the whole sup chain and so
  // reaches the blanket "sup Copy ext Drop" for every copyable type. That impl exists so a "Drop" constraint accepts
  // a number, not so that anything is emitted for one - its "self" is typed at "Copy", which no concrete value can
  // be passed as by value.
  if (type_sym.IsCopyable()) { return false; }

  // A destructor of its own settles it without having to
  // look at the attributes at all.
  if (FindDropOverload(type_sym, sm, meta) != nullptr) { return true; }

  // Otherwise the type is only worth dropping if something
  // it holds is. A type cannot contain itself by value, so the
  // recursion is bounded by the nesting depth of the type.
  return genex::any_of(
    GetAllAttrs(*type_sym.FqName(), sm), [&](auto const &attr) {
      const auto attr_type_sym = std::get<1>(attr);
      return attr_type_sym != nullptr and attr_type_sym != &type_sym and NeedsDrop(*attr_type_sym, sm, meta);
    });
}
