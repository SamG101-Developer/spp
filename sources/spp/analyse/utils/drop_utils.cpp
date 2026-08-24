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

auto spp::analyse::utils::drop_utils::FindDelOverload(
  scopes::TypeSymbol const &type_sym,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> asts::FunctionPrototypeAst* {
  //
  using type_utils::TypeEq;
  using asts::generate::common_types_precompiled::DEL;

  // A generic that was never bound, or a symbol with no
  // scope of its own, has no attributes and no methods
  // to find. Todo: What if we constrain generic with Del?
  if (type_sym.LinkedScope == nullptr) { return nullptr; }

  // The type only has a destructor if it superimposes
  // "Del" *directly*. This is checked before looking
  // for the method, because a class is free to declare
  // a method called "del" without meaning this at all.
  const auto superimposes_del = genex::any_of(
    type_sym.LinkedScope->DirectSupScopes, [&](auto const *sup_scope) {
      if (sup_scope->TySym == nullptr) { return false; }
      return TypeEq(*sup_scope->TySym->FqName(), *DEL, *sup_scope, *sm.CurrentScope);
    });
  if (not superimposes_del) { return nullptr; }

  // Find the "del" the type actually inherits.
  // "GetAllFunctionScopes" searches the sup scopes,
  // so an override on the type itself and an
  // implementation inherited from a type it extends
  // are both found here.
  const auto del_name = asts::IdentifierAst(0, "del");
  const auto overloads = func_utils::GetAllFunctionScopes(
    del_name, type_sym.LinkedScope, sm, meta);

  for (auto const &overload : overloads) {
    // "Del::del" itself is abstract with an empty body:
    // a type that superimposes "Del" but never overrides
    // "del" resolves to it, and calling it would be a call
    // into nothing.
    if (overload.Proto->AbstractAnnotation != nullptr) { continue; }

    // The destructor is the "&mut self" overload taking
    // nothing else. Anything else called "del" is an unrelated
    // method that happens to share the name.
    const auto self_param = overload.Proto->FnParamGroup->GetSelfParam();
    if (self_param == nullptr or self_param->Conv == nullptr) { continue; }
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
  if (type_utils::IsTypeGen(*type_sym.FqName(), *sm.CurrentScope)) { return true; }

  // A destructor of its own settles it without having to
  // look at the attributes at all.
  if (FindDelOverload(type_sym, sm, meta) != nullptr) { return true; }

  // Otherwise the type is only worth dropping if something
  // it holds is. A type cannot contain itself by value, so the
  // recursion is bounded by the nesting depth of the type.
  return genex::any_of(
    GetAllAttrs(*type_sym.FqName(), sm), [&](auto const &attr) {
      const auto attr_type_sym = std::get<1>(attr);
      return attr_type_sym != nullptr and attr_type_sym != &type_sym and NeedsDrop(*attr_type_sym, sm, meta);
    });
}
