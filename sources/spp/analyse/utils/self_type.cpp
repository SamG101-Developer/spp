module;
#include <spp/macros.hpp>

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

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, GenericSubst);
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeId);
use_alias(spp::analyse::scopes, TypeKind);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeIdentifierAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);

auto spp::analyse::utils::self_type::SubstituteSelf(
  TypeAst const &type, TypeAst const *self, Scope const &scope,
  ScopeManager *const sm, CompilerMetaData *const meta) -> Shared<TypeAst> {
  if (self == nullptr or not type_predicates::DoesTypeNameSelf(type)) { return AstClone(&type); }
  auto bindings = GenericSubst();
  BindSelf(bindings, *self, scope);
  auto out = type_resolution::ReadType(type, ExprSubst::In(scope, std::move(bindings)));
  if (sm == nullptr) { return out; }
  const auto _meta_guard = MetaGuard(meta);
  meta->AllowAbstractType = true;
  out->Stage7_AnalyseSemantics(sm, meta);
  return out;
}

auto spp::analyse::utils::self_type::TypeIdWithSelf(
  const TypeId instance_id, TypeAst const &written, Scope const *const written_in,
  const TypeId self) -> TypeId {
  auto id = instance_id;
  if (id == nullptr and written_in != nullptr and self != nullptr and type_predicates::DoesTypeNameSelf(written)) {
    id = written_in->TypeIdOf(written);
  }
  return SubstituteSelfId(id, self);
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

auto spp::analyse::utils::self_type::RegisterSelf(
  TypeAst const &name, ScopeManager &sm) -> void {
  if (name.IsCompilerGeneratedType()) { return; }

  // A block over an alias ("sup U8") is over what the alias
  // stands for: its "Self" is the target, not the class at
  // the end of the alias's chain (which only links the template).
  auto *sym = sm.CurrentScope->FindTypeSymbol(&name);
  if (sym->Alias != nullptr) { sym = sym->AliasTarget(*sm.CurrentScope); }
  AddSelfTypeSymbol(*sm.CurrentScope, sym->LinkedScope, name.PosStart());
}

auto spp::analyse::utils::self_type::RegisterProvisionalSelf(
  TypeAst const &name, ScopeManager &sm) -> void {
  if (name.IsCompilerGeneratedType()) { return; }
  if (const auto base_sym = sm.CurrentScope->FindHeadSymbol(name)) {
    AddSelfTypeSymbol(*sm.CurrentScope, base_sym->LinkedScope, name.PosStart());
  }
}
