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

namespace {
  using spp::analyse::scopes::TypeSymbol;
  using spp::analyse::scopes::VariableSymbol;

  /// A pack is a generic symbol that is variadic: a parameter ("Ts" for "..Ts"), or the argument binding one.
  auto IsTypePack(TypeSymbol const &sym) -> bool {
    return sym.IsGn() and sym.IsVariadic;
  }

  auto IsCompPack(VariableSymbol const &sym) -> bool {
    return sym.IsGn() and sym.IsVariadic;
  }

  /// The comp pack an identifier names, if it names one.
  auto FindCompPack(spp::asts::ExpressionAst const &value, spp::analyse::scopes::Scope const &scope)
    -> VariableSymbol const* {
    auto const *const id = value.To<spp::asts::IdentifierAst>();
    auto const *const sym = id != nullptr ? scope.FindVarSymbol(id) : nullptr;
    return sym != nullptr and IsCompPack(*sym) ? sym : nullptr;
  }

  /// "FindCompPack" for a type pack.
  auto FindTypePack(spp::asts::TypeAst const &type, spp::analyse::scopes::Scope const &scope) -> TypeSymbol* {
    auto *const sym = scope.FindHeadSymbol(type);
    return sym != nullptr and IsTypePack(*sym) ? sym : nullptr;
  }
}

auto spp::analyse::utils::packs::TypePackElements(
  TypeAst const &pack)
  -> Vec<Shared<TypeAst>> {
  return pack.LastTypePart()->GnArgGroup->GetTypeArgs()
    | genex::views::transform([](auto const *arg) { return arg->TypeVal; })
    | genex::to<Vec>();
}

auto spp::analyse::utils::packs::CompPackElements(
  ExpressionAst const &pack)
  -> Vec<ExpressionAst*> {
  return pack.ToUnchecked<asts::TupleLiteralAst>()->Elems
    | genex::views::transform([](auto const &elem) { return elem.get(); })
    | genex::to<Vec>();
}

auto spp::analyse::utils::packs::BoundTypePackElements(
  TypeAst const &type,
  Scope const &scope)
  -> std::optional<Vec<Shared<TypeAst>>> {
  if (not type.IsTypeIdentifier() or not type.LastTypePart()->GnArgGroup->Args.IsEmpty()) { return std::nullopt; }
  auto *const sym = FindTypePack(type, scope);
  if (sym == nullptr or not type_predicates::IsTypeTuple(TypeRef::OfKind(*sym, scope), scope)) { return std::nullopt; }
  return TypePackElements(*sym->FqName());
}

auto spp::analyse::utils::packs::BoundCompPackElements(
  ExpressionAst const &value,
  Scope const &scope)
  -> std::optional<Vec<ExpressionAst*>> {
  auto const *const sym = FindCompPack(value, scope);
  auto const *const bound = sym != nullptr ? sym->AsBound(scope)->BoundCompVal() : nullptr;
  if (bound == nullptr or bound->To<asts::TupleLiteralAst>() == nullptr) { return std::nullopt; }
  return CompPackElements(*bound);
}

auto spp::analyse::utils::packs::DoesTypeNameAnUnboundPack(
  TypeAst const &type,
  Scope const &scope)
  -> bool {
  auto *const sym = FindTypePack(type, scope);
  return sym != nullptr and sym->AsBound()->IsGn();
}

auto spp::analyse::utils::packs::DoesCompNameAnUnboundPack(
  ExpressionAst const &value,
  Scope const &scope)
  -> bool {
  auto const *const sym = FindCompPack(value, scope);
  return sym != nullptr and sym->AsBound(scope)->BoundCompVal() == nullptr;
}

auto spp::analyse::utils::packs::DoesArgNameAPack(
  GenericArgumentAst const &arg,
  Scope const &scope)
  -> bool {
  return arg.IsTypeArg() ? FindTypePack(*arg.TypeVal, scope) != nullptr : FindCompPack(*arg.CompVal, scope) != nullptr;
}

auto spp::analyse::utils::packs::DoesArgNameAnUnboundPack(
  GenericArgumentAst const &arg,
  Scope const &scope)
  -> bool {
  return arg.IsTypeArg()
    ? DoesTypeNameAnUnboundPack(*arg.TypeVal, scope)
    : DoesCompNameAnUnboundPack(*arg.CompVal, scope);
}

auto spp::analyse::utils::packs::PackTypeParamName(
  IdentifierAst const &param_name)
  -> Str {
  return "VariadicPackOf" + param_name.Val;
}
