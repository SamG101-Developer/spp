module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.self_type;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.generic_argument_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import std;

auto spp::analyse::utils::self_type::SubstituteSelfType(
  TypeAst const &type,
  Scope const &scope,
  meta::CompilerMetaData const &meta,
  bool *const substituted)
  -> Shared<TypeAst> {
  // Substitute "Self" with the concrete enclosing type, if there is one and the type names it.
  const auto true_self_type = scope.GetEnclosingSelfType(meta);
  if (true_self_type == nullptr or not type_predicates::NamesSelfType(type)) { return AstClone(&type); }
  if (substituted != nullptr) { *substituted = true; }
  return SubstituteSelfTypeWith(type, *true_self_type);
}

auto spp::analyse::utils::self_type::SubstituteSelfTypeAndAnalyse(
  TypeAst const &type,
  Scope const &scope,
  ScopeManager &sm,
  meta::CompilerMetaData &meta,
  bool *const substituted)
  -> Shared<TypeAst> {
  auto replaced = false;
  auto t = SubstituteSelfType(type, scope, meta, &replaced);
  if (substituted != nullptr) { *substituted = replaced; }

  // Only a type that actually had a "Self" replaced is analysed here. One that did not is handed back as the plain
  // clone it is, so that this does not analyse a written type at a point its owner has not chosen to - and so that a
  // "Self" left standing for want of an enclosing type is reported by whoever does analyse it.
  if (not replaced) { return t; }

  const auto _meta_guard = meta::MetaGuard(&meta);
  meta.AllowAbstractType = true;
  t->Stage7_AnalyseSemantics(&sm, &meta);
  return t;
}

auto spp::analyse::utils::self_type::SubstituteSelfTypeWith(
  TypeAst const &type,
  TypeAst const &replacement)
  -> Shared<TypeAst> {
  using generate::common_types::SelfType;

  // If "Self" is not present, return a plain clone.
  if (not type_predicates::NamesSelfType(type)) { return AstClone(&type); }

  const auto g = GenericArgumentAst::NewType(
    SelfType(0), AstClone(&replacement));
  const auto args = Vec<GenericArgumentAst*>{g.get()};
  return type.SubstituteGenerics(args);
}
