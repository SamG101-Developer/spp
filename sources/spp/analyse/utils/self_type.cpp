module spp.analyse.utils.self_type;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.generic_argument_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import std;

auto spp::analyse::utils::self_type::SubstituteSelf(
  TypeAst const &type, TypeAst const *self, Scope const &scope,
  ScopeManager *const sm, meta::CompilerMetaData *const meta) -> Shared<TypeAst> {
  if (self == nullptr or not type_predicates::DoesTypeNameSelf(type)) { return AstClone(&type); }
  auto bindings = GenericSubst();
  BindSelf(bindings, *self, scope);
  auto out = type_resolution::ReadType(type, ExprSubst::In(scope, std::move(bindings)));
  if (sm == nullptr) { return out; }
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->AllowAbstractType = true;
  out->Stage7_AnalyseSemantics(sm, meta);
  return out;
}
