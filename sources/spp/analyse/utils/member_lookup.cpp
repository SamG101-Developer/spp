module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.member_lookup;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_identifier_ast;
import spp.utils.ptr;
import spp.utils.strings;
import genex;
import std;

namespace spp::analyse::utils::member_lookup {
  namespace {
    /// The body that both the wrapper function "ScopesDeclaringVar"
    /// and "ScopesDeclaringType" use; walk the type's own scope and
    /// its superimpositions, asking each one for the name and
    /// record how far it sits from where the lookup began.
    template <typename Entry, typename Name, typename Lookup>
    auto ScopesDeclaring(
      Scope &type_scope, Name const &name, Lookup &&lookup) -> Vec<Entry> {
      // Build a vector of the type scope and all of its super scopes
      // (of any level).
      auto scopes = Vec{&type_scope};
      scopes.AppendRange(type_scope.SupScopes());

      // If we discover the symbol within the scope, keep it, otherwise
      // discard. We are left only with the scopes that found contain
      // the symbol directly.
      auto out = Vec<Entry>();
      for (const auto scope : scopes) {
        if (const auto sym = lookup(*scope, name); sym != nullptr) {
          out.EmplaceBack(Entry{.Depth = type_scope.DepthDiff(scope), .Where = scope, .Symbol = sym});
        }
      }
      return out;
    }

    /// The body both the "ClosestScopes" wrap.
    template <typename Entry>
    auto ClosestScopesImpl(
      Vec<Entry> const &candidates) -> Vec<Entry> {
      // Find the minimum depth from all the provided info
      // blocks. This is the depth that must be unique for
      // a non-ambiguous lookup.
      if (candidates.IsEmpty()) { return {}; }
      auto min_depth = candidates[0].Depth;
      for (auto const &c : candidates) { min_depth = std::min(min_depth, c.Depth); }

      // Only keep the info blocks whose depth matches the
      // minimum depth. This could be any number of the
      // scopes, and we check the count for an error later.
      auto out = Vec<Entry>();
      for (auto const &c : candidates) {
        if (c.Depth == min_depth) { out.EmplaceBack(c); }
      }
      return out;
    }

    /// The body both the "RaiseIfAmbiguous" wrap.
    template <typename Entry>
    auto RaiseIfAmbiguousImpl(
      Vec<Entry> const &closest, Ast const &access,
      ScopeManager const &sm) -> void {
      // If there is a maximum of 1 scope containing the
      // target identifier/type-identifier, then return.
      // Otherwise, there is an ambiguity, so raise an
      // error.
      using errors::SppAmbiguousMemberAccessError;
      if (closest.Len() <= 1) { return; }
      Raise<SppAmbiguousMemberAccessError>(
        {closest[0].Where, closest[1].Where, sm.CurrentScope},
        ERR_ARGS(*closest[0].Symbol->Name, *closest[1].Symbol->Name, access));
    }
  }
}

auto spp::analyse::utils::member_lookup::RaiseMissingIdentifierAndClosestOptions(
  IdentifierAst const &identifier, Vec<VariableSymbol*> const &var_symbols,
  Vec<NamespaceSymbol*> const &ns_symbols, ScopeManager const &sm) -> void {
  using errors::SppIdentifierUnknownError;
  using spp::utils::strings::ClosestMatch;

  //
  const auto v_alternatives = var_symbols
    | genex::views::transform([](auto const &x) { return x->Name->Val; })
    | genex::to<Vec>();
  const auto ns_alternatives = ns_symbols
    | genex::views::transform([](auto const &x) { return x->Name->Val; })
    | genex::to<Vec>();
  const auto alternatives = genex::views::concat(v_alternatives, ns_alternatives) | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Val, alternatives);
  Raise<SppIdentifierUnknownError>(
    {sm.CurrentScope}, ERR_ARGS(identifier, "identifier", closest_match));
}

auto spp::analyse::utils::member_lookup::RaiseMissingTypeIdentifierAndClosestOptions(
  TypeIdentifierAst const &identifier, Vec<TypeSymbol*> const &symbols,
  ScopeManager const &sm) -> void {
  using spp::utils::strings::ClosestMatch;

  //
  const auto alternatives = symbols
    | genex::views::filter([](auto const &x) { return not x->IsMock(); })
    | genex::views::transform([](auto const &x) { return x->Name->Name; })
    | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Name, alternatives);
  Raise<errors::SppIdentifierUnknownError>(
    {sm.CurrentScope}, ERR_ARGS(identifier, "type identifier", closest_match));
}

auto spp::analyse::utils::member_lookup::GetTypeSymOrError(
  Scope const &scope,
  TypeIdentifierAst const &type_part,
  ScopeManager const &sm)
  -> TypeSymbol* {
  //
  using member_lookup::RaiseMissingTypeIdentifierAndClosestOptions;

  // Get the type part's symbol, and raise an error if it doesn't exist.
  const auto type_sym = scope.GetTypeSymbol(&type_part, false);
  if (type_sym == nullptr) {
    RaiseMissingTypeIdentifierAndClosestOptions(type_part, scope.AllTypeSymbols(), sm);
  }

  // Return the found type symbol.
  return type_sym;
}

auto spp::analyse::utils::member_lookup::GetNsScopeOrError(
  Scope const &scope,
  IdentifierAst const &ns,
  ScopeManager const &sm)
  -> Scope* {
  //
  using member_lookup::RaiseMissingIdentifierAndClosestOptions;

  // If the namespace does not exist, raise an error.
  const auto ns_sym = scope.GetNsSymbol(&ns);
  if (ns_sym == nullptr) {
    RaiseMissingIdentifierAndClosestOptions(ns, {}, scope.AllNsSymbols(), sm);
  }

  // Return the found namespace scope.
  return ns_sym->LinkedScope;
}

auto spp::analyse::utils::member_lookup::MemberReachableBy(
  VariableSymbol const &sym, const MemberAccessForm form) -> bool {
  // A method is reached either way: how it is called (static
  // or runtime) is what decides whether it errors. Anything
  // with no runtime storage of its own is reached statically,
  // and the rest - attributes - at runtime.
  if (sym.Kind == VariableKind::Function) { return true; }
  return sym.IsCompTime() == (form == MemberAccessForm::Static);
}

auto spp::analyse::utils::member_lookup::MembersReachableBy(
  Vec<DeclaringVarScope> const &candidates,
  const MemberAccessForm form) -> Vec<DeclaringVarScope> {
  auto out = Vec<DeclaringVarScope>();
  for (auto const &candidate : candidates) {
    if (MemberReachableBy(*candidate.Symbol, form)) { out.EmplaceBack(candidate); }
  }
  return out;
}

auto spp::analyse::utils::member_lookup::LookupMemberForAccess(
  Scope &type_scope, IdentifierAst const &name,
  const MemberAccessForm form) -> VariableSymbol* {
  // Get all the scopes that declare this variable (as a field),
  // keep the ones this form can reach, and take the nearest.
  const auto closest = ClosestScopes(
    MembersReachableBy(ScopesDeclaringVar(type_scope, name, false), form));
  return closest.IsEmpty() ? nullptr : closest[0].Symbol;
}

auto spp::analyse::utils::member_lookup::ScopesDeclaringVar(
  Scope &type_scope, IdentifierAst const &name,
  const bool sup_scope_search) -> Vec<DeclaringVarScope> {
  // So a super scope search filtered to the scopes
  // that contain the variable symbol specified by
  // the identifier.
  return ScopesDeclaring<DeclaringVarScope>(
    type_scope, name, [sup_scope_search](Scope const &scope, IdentifierAst const &n) {
      return scope.GetVarSymbol(&n, true, sup_scope_search);
    });
}

auto spp::analyse::utils::member_lookup::ScopesDeclaringType(
  Scope &type_scope, TypeIdentifierAst const &name,
  const bool sup_scope_search) -> Vec<DeclaringTypeScope> {
  // So a super scope search filtered to the scopes
  // that contain the type symbol specified by the
  // type identifier.
  return ScopesDeclaring<DeclaringTypeScope>(
    type_scope, name, [sup_scope_search](Scope const &scope, TypeIdentifierAst const &n) {
      return scope.GetTypeSymbol(&n, true, sup_scope_search);
    });
}

auto spp::analyse::utils::member_lookup::ClosestScopes(
  Vec<DeclaringVarScope> const &candidates)
  -> Vec<DeclaringVarScope> {
  // Wrap the implementation function to filter to
  // the info blocks containing minimum depth scopes.
  return ClosestScopesImpl(candidates);
}

auto spp::analyse::utils::member_lookup::ClosestScopes(
  Vec<DeclaringTypeScope> const &candidates)
  -> Vec<DeclaringTypeScope> {
  // Wrap the implementation function to filter to
  // the info blocks containing minimum depth scopes.
  return ClosestScopesImpl(candidates);
}

auto spp::analyse::utils::member_lookup::RaiseIfAmbiguous(
  Vec<DeclaringVarScope> const &closest,
  Ast const &access, ScopeManager const &sm) -> void {
  // Wrap the implementation function to raise an error
  // if there are more than 1 scopes at the equal minimum
  // level => ambiguous lookup.
  RaiseIfAmbiguousImpl(closest, access, sm);
}

auto spp::analyse::utils::member_lookup::RaiseIfAmbiguous(
  Vec<DeclaringTypeScope> const &closest,
  Ast const &access, ScopeManager const &sm) -> void {
  // Wrap the implementation function to raise an error
  // if there are more than 1 scopes at the equal minimum
  // level => ambiguous lookup.
  RaiseIfAmbiguousImpl(closest, access, sm);
}
