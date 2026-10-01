module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.packs;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;

/// [CHECKED]
auto spp::analyse::utils::packs::IsTypePack(TypeSymbol const &sym) -> bool {
  // A type pack is both generic and variadic.
  return sym.IsTypeGeneric() and sym.IsVariadic;
}

/// [CHECKED]
auto spp::analyse::utils::packs::IsCompPack(VariableSymbol const &sym) -> bool {
  // A comp pack is both generic and variadic.
  return sym.IsCompGeneric() and sym.IsVariadic;
}

auto spp::analyse::utils::packs::PackElementTypes(
  TypeAst const &pack)
  -> Vec<Shared<TypeAst>> {
  return pack.LastTypePart()->GnArgGroup->GetTypeArgs()
    | genex::views::transform([](auto const *arg) { return arg->TypeVal; })
    | genex::to<Vec>();
}

auto spp::analyse::utils::packs::PackElementValues(
  ExpressionAst const &pack)
  -> Vec<ExpressionAst*> {
  auto const *const tup = pack.To<asts::TupleLiteralAst>();
  if (tup == nullptr) { return {}; }
  return tup->Elems
    | genex::views::transform([](auto const &elem) -> ExpressionAst* { return elem.get(); })
    | genex::to<Vec>();
}

/// [CHECKED]
auto spp::analyse::utils::packs::BoundPackTypes(
  TypeAst const &type,
  Scope const &scope)
  -> std::optional<Vec<Shared<TypeAst>>> {
  if (not type.IsTypeIdentifier() or not type.LastTypePart()->GnArgGroup->Args.IsEmpty()) { return std::nullopt; }
  auto *const sym = scope.GetTypeSymbol(&type);
  if (sym == nullptr or sym->Kind != scopes::TypeKind::GenericArg or not sym->IsVariadic) { return std::nullopt; }
  auto *const bound = sym->AsBoundSymbol();
  if (bound == sym or not type_predicates::IsTypeTup(*bound, scope)) { return std::nullopt; }
  return PackElementTypes(*sym->FqName());
}

auto spp::analyse::utils::packs::BoundPackValues(
  ExpressionAst const &value,
  Scope const &scope)
  -> std::optional<Vec<ExpressionAst*>> {
  auto const *const id = value.To<IdentifierAst>();
  auto const *const sym = id != nullptr ? scope.GetVarSymbol(id) : nullptr;
  if (sym == nullptr or sym->Kind != scopes::VariableKind::GenericCompArg or not sym->IsVariadic) {
    return std::nullopt;
  }
  auto const *const bound = sym->BoundCompValue();
  if (bound == nullptr or bound->To<asts::TupleLiteralAst>() == nullptr) { return std::nullopt; }
  return PackElementValues(*bound);
}

auto spp::analyse::utils::packs::IsUnboundPackNamed(
  GenericArgumentAst const &arg,
  Scope const &scope)
  -> bool {
  if (arg.CompVal != nullptr) { return IsUnboundCompPackNamed(*arg.CompVal, scope); }
  if (arg.TypeVal == nullptr) { return false; }
  auto *const sym = scope.GetTypeSymbol(arg.TypeVal->WithoutGenerics().get(), false);
  return sym != nullptr and IsTypePack(*sym) and sym->AsBoundSymbol() == sym;
}

auto spp::analyse::utils::packs::IsUnboundCompPackNamed(
  ExpressionAst const &value,
  Scope const &scope)
  -> bool {
  auto const *const id = value.To<IdentifierAst>();
  auto const *const sym = id != nullptr ? scope.GetVarSymbol(id) : nullptr;
  return sym != nullptr and sym->Kind == scopes::VariableKind::GenericCompParam and IsCompPack(*sym);
}

auto spp::analyse::utils::packs::IsPackParam(
  const std::uint64_t param_id) -> bool {
  if (param_id == 0) { return false; }

  // Handle a variadic type parameter. Check on the symbol
  // level for the variadic flag.
  if (const auto type_param = scopes::GenericParamOf(param_id); type_param != nullptr) {
    return type_param->IsVariadic;
  }

  // Handle a variadic comp parameter. Check on the symbol
  // level for the variadic flag.
  if (const auto comp_param = scopes::GenericCompParamOf(param_id); comp_param != nullptr) {
    return comp_param->IsVariadic;
  }

  // Nullptr safeguard (this should never be hit), but is
  // needed for the C++ type system.
  return false;
}

auto spp::analyse::utils::packs::PackTypeParamName(
  IdentifierAst const &param_name)
  -> Str {
  return "VariadicPackOf" + param_name.Val;
}

auto spp::analyse::utils::packs::IsUnboundPack(
  VariableSymbol const &sym,
  Scope const &scope)
  -> bool {
  if (not sym.IsVariadic) { return false; }

  // A comp pack is bound by an instantiation's argument symbol,
  // which replaces the parameter's.
  if (sym.Kind == scopes::VariableKind::GenericCompParam) { return true; }
  if (sym.Kind == scopes::VariableKind::GenericCompArg) { return false; }

  // A function parameter pack is bound where its instantiation
  // declares the pack's type parameter.
  const auto pack_type = MakeUnique<asts::TypeIdentifierAst>(0, PackTypeParamName(*sym.Name), nullptr);
  return scope.GetTypeSymbol(pack_type.get()) == nullptr;
}

auto spp::analyse::utils::packs::NamesPack(
  GenericArgumentAst const &arg, Scope const &scope)
  -> bool {
  if (arg.TypeVal != nullptr) {
    const auto sym = scope.GetTypeSymbol(arg.TypeVal->WithoutGenerics().get(), false);
    return sym != nullptr and packs::IsTypePack(*sym);
  }
  auto const *const name = arg.CompVal != nullptr ? arg.CompVal->To<IdentifierAst>() : nullptr;
  auto const *const sym = name != nullptr ? scope.GetVarSymbol(name, false) : nullptr;
  return sym != nullptr and packs::IsCompPack(*sym);
}
