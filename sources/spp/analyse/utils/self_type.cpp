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

auto spp::analyse::utils::self_type::SubstituteSelf(
  TypeAst const &type,
  TypeAst const *const self,
  ScopeManager *const sm,
  meta::CompilerMetaData *const meta)
  -> Shared<TypeAst> {
  if (self == nullptr or not type_predicates::DoesTypeNameSelf(type)) { return AstClone(&type); }
  auto out = type.SubstituteSelf(*self);
  if (sm == nullptr) { return out; }
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->AllowAbstractType = true;
  out->Stage7_AnalyseSemantics(sm, meta);
  return out;
}
