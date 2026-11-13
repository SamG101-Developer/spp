module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.packs;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::asts, AstCloneShared);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, TupleLiteralAst);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeIdentifierAst);

namespace spp::analyse::utils::packs {
  using spp::analyse::scopes::TypeSymbol;
  using spp::analyse::scopes::VariableSymbol;

  /// A pack is a generic symbol that is variadic: a parameter
  // ("Ts" for "..Ts"), or the argument binding one.
  static auto IsTypePack(
    TypeSymbol const &sym) -> bool;

  static auto IsCompPack(
    VariableSymbol const &sym) -> bool;

  /// The comp pack an identifier names, if it names one.
  static auto FindCompPack(
    ExpressionAst const &value, Scope const &scope)
    -> VariableSymbol const*;

  /// "FindCompPack" for a type pack.
  static auto FindTypePack(
    TypeAst const &type, Scope const &scope) -> TypeSymbol*;
}

auto spp::analyse::utils::packs::IsTypePack(
  TypeSymbol const &sym) -> bool {
  return sym.IsGn() and sym.IsVariadic;
}

auto spp::analyse::utils::packs::IsCompPack(
  VariableSymbol const &sym) -> bool {
  return sym.IsGn() and sym.IsVariadic;
}

auto spp::analyse::utils::packs::FindCompPack(
  ExpressionAst const &value, Scope const &scope)
  -> VariableSymbol const* {
  auto const *const id = value.To<IdentifierAst>();
  auto const *const sym = id != nullptr ? scope.FindVarSymbol(id) : nullptr;
  return sym != nullptr and IsCompPack(*sym) ? sym : nullptr;
}

auto spp::analyse::utils::packs::FindTypePack(
  TypeAst const &type, Scope const &scope) -> TypeSymbol* {
  auto *const sym = scope.FindHeadSymbol(type);
  return sym != nullptr and IsTypePack(*sym) ? sym : nullptr;
}

auto spp::analyse::utils::packs::CompPackElements(
  ExpressionAst const &pack) -> Vec<ExpressionAst*> {
  return pack.As<TupleLiteralAst>()->Elems
    | genex::views::transform([](auto const &elem) { return elem.get(); })
    | genex::to<Vec>();
}

auto spp::analyse::utils::packs::BoundCompPackElements(
  ExpressionAst const &value, Scope const &scope) -> Opt<Vec<ExpressionAst*>> {
  //
  auto const *const sym = FindCompPack(value, scope);
  auto const *const bound = sym != nullptr ? sym->AsBound()->BoundCompVal() : nullptr;
  if (bound == nullptr or bound->To<TupleLiteralAst>() == nullptr) { return Nullopt; }
  return CompPackElements(*bound);
}

auto spp::analyse::utils::packs::DoesTypeNameAnUnboundPack(
  TypeAst const &type, Scope const &scope) -> bool {
  // A pack bound to its list links to nothing ("TypeSymbol::BoundPack"), so is bound though it reads as generic.
  const auto sym = FindTypePack(type, scope);
  return sym != nullptr and sym->AsBound()->IsGn() and sym->AsBound()->BoundPack == nullptr;
}

auto spp::analyse::utils::packs::FnVariadicParamName(
  IdentifierAst const &param_name)
  -> Str {
  return "VariadicPackOf" + param_name.Val;
}

auto spp::analyse::utils::packs::DeclareHiddenPack(
  Str const &name, TypeAst const &element, void const *key, const std::size_t pos, Scope &scope)
  -> Shared<TypeIdentifierAst> {
  using scopes::TypeSymbol;
  auto hidden = MakeShared<TypeIdentifierAst>(pos, Str(name), nullptr);
  const auto written = AstCloneShared(element.WithoutConvention());
  auto const *const found = scope.FindTypeSymbol(written.get());
  auto const *const declared = found != nullptr ? found->AsBound() : nullptr;
  auto constraints = declared == nullptr ? Vec<Shared<TypeAst>>() : declared->IsGn()
    ? declared->TypeConstraints
    : Vec<Shared<TypeAst>>{AstCloneShared(declared->FqName())};

  const auto sym = MakeShared<TypeSymbol>(
    AstCloneShared(hidden), nullptr, nullptr, &scope, scopes::TypeKind::GnTypeParam, false,
    asts::utils::Visibility::kPublic, asts::ConventionTag::MOV, std::move(constraints));
  sym->IsVariadic = true;
  sym->OwnParamId = scopes::GnPackParamId(key, name);
  if (declared != nullptr) { sym->DerivesFromSymbol = const_cast<TypeSymbol&>(*declared).SharedFromThis<TypeSymbol>(); }
  scopes::RegisterGnTypeParam(*sym);
  hidden->StampTypeId(scopes::NameTypeIdOf(*sym));
  sym->Name->StampTypeId(scopes::NameTypeIdOf(*sym));
  scope.AddTypeSymbol(sym);
  return hidden;
}

auto spp::analyse::utils::packs::PackElementName(
  IdentifierAst const &pack_name, const std::size_t index) -> Str {
  return pack_name.Val + "#" + std::to_string(index);
}
