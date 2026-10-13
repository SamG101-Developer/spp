module spp.analyse.utils.self_type;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
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

auto spp::analyse::utils::self_type::MakeSelfTypeSymbol(
  Scope *linked_scope, Scope *defined_in, const std::size_t pos) -> Shared<TypeSymbol> {
  // Create a "Self" symbol with a provided set of scopes.
  return MakeShared<TypeSymbol>(
    MakeUnique<TypeIdentifierAst>(pos, "Self", nullptr),
    nullptr, linked_scope, defined_in, TypeKind::Self);
}

auto spp::analyse::utils::self_type::AddSelfTypeSymbol(
  Scope &defined_in, Scope *linked_scope, const std::size_t pos) -> void {
  // "defined_in" is where the symbol lives, not necessarily the
  // scope of the type it stands for ("linked_scope").
  if (linked_scope == nullptr) { return; }
  defined_in.AddTypeSymbol(MakeSelfTypeSymbol(linked_scope, &defined_in, pos));
}
