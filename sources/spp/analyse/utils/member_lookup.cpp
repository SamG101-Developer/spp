module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.member_lookup;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.visibility_utils;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.strings;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, NamespaceSymbol);
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::analyse::scopes, VariableKind);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts, Ast);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeIdentifierAst);
use_alias(spp::asts::meta, CompilerMetaData);

namespace spp::analyse::utils::member_lookup {
  /// The body that both the wrapper function "ScopesDeclaringVar"
  /// and "ScopesDeclaringType" use; walk the type's own scope and
  /// its superimpositions, asking each one for the name and
  /// record how far it sits from where the lookup began.
  template <typename Entry, typename Name, typename Lookup>
  static auto ScopesDeclaring(
    Scope &type_scope, Name const &name, Lookup &&lookup) -> Vec<Entry>;

  /// The body both the "ClosestScopes" wrap.
  template <typename Entry>
  static auto ClosestScopesImpl(
    Vec<Entry> const &candidates) -> Vec<Entry>;

  /// The body both the "RaiseIfAmbiguous" wrap.
  template <typename Entry>
  static auto RaiseIfAmbiguousImpl(
    Vec<Entry> const &closest, Ast const &access) -> void;

  /// Raise an error for a missing type identifier, and provide
  /// the closest options; a singular place to uniformly handle
  /// these errors.
  SPP_ATTR_COLD SPP_ATTR_NORETURN static auto RaiseMissingTypeIdentifierAndClosestOptions(
    TypeIdentifierAst const &identifier, Vec<TypeSymbol*> const &symbols) -> void;
}

template <typename Entry, typename Name, typename Lookup>
auto spp::analyse::utils::member_lookup::ScopesDeclaring(
  Scope &type_scope, Name const &name, Lookup &&lookup) -> Vec<Entry> {
  // Build a vector of the type scope and all of its super scopes
  // (of any level).
  auto scopes = Vec{&type_scope};
  scopes.append_range(type_scope.GetSupScopes());

  // If we discover the symbol within the scope, keep it, otherwise
  // discard. We are left only with the scopes that found contain
  // the symbol directly.
  auto out = Vec<Entry>();
  for (const auto scope : scopes) {
    if (const auto sym = lookup(*scope, name); sym != nullptr) {
      out.emplace_back(Entry{.Depth = type_scope.DepthDiff(scope), .Where = scope, .Symbol = sym});
    }
  }
  return out;
}

template <typename Entry>
auto spp::analyse::utils::member_lookup::ClosestScopesImpl(
  Vec<Entry> const &candidates) -> Vec<Entry> {
  // Find the minimum depth from all the provided info
  // blocks. This is the depth that must be unique for
  // a non-ambiguous lookup.
  if (candidates.empty()) { return {}; }
  auto min_depth = candidates[0].Depth;
  for (auto const &c : candidates) { min_depth = std::min(min_depth, c.Depth); }

  // Only keep the info blocks whose depth matches the
  // minimum depth. This could be any number of the
  // scopes, and we check the count for an error later.
  auto out = Vec<Entry>();
  for (auto const &c : candidates) {
    if (c.Depth == min_depth) { out.emplace_back(c); }
  }
  return out;
}

template <typename Entry>
auto spp::analyse::utils::member_lookup::RaiseIfAmbiguousImpl(
  Vec<Entry> const &closest, Ast const &access) -> void {
  // If there is a maximum of 1 scope containing the
  // target identifier/type-identifier, then return.
  // Otherwise, there is an ambiguity, so raise an
  // error.
  using errors::SppAmbiguousMemberAccessError;
  if (closest.size() <= 1) { return; }
  Raise<SppAmbiguousMemberAccessError>(ERR_ARGS(*closest[0].Symbol->Name, *closest[1].Symbol->Name, access));
}

SPP_ATTR_COLD SPP_ATTR_NORETURN auto spp::analyse::utils::member_lookup::RaiseMissingTypeIdentifierAndClosestOptions(
  TypeIdentifierAst const &identifier, Vec<TypeSymbol*> const &symbols) -> void {
  using spp::utils::strings::ClosestMatch;

  //
  const auto alternatives = symbols
    | genex::views::filter([](auto const &x) { return not x->IsMock(); })
    | genex::views::transform([](auto const &x) { return x->Name->Name; })
    | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Name, alternatives);
  Raise<errors::SppIdentifierUnknownError>(ERR_ARGS(identifier, "type identifier", closest_match));
}

auto spp::analyse::utils::member_lookup::RaiseMissingIdentifierAndClosestOptions(
  IdentifierAst const &identifier, Vec<VariableSymbol*> const &var_symbols,
  Vec<NamespaceSymbol*> const &ns_symbols) -> void {
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
  Raise<SppIdentifierUnknownError>(ERR_ARGS(identifier, "identifier", closest_match));
}

auto spp::analyse::utils::member_lookup::FindTypeSymbolOrError(
  Scope const &scope,
  TypeAst const &type)
  -> TypeSymbol* {
  const auto sym = scope.FindTypeSymbol(&type);
  if (sym == nullptr) {
    RaiseMissingTypeIdentifierAndClosestOptions(*type.LastTypePart(), scope.GetAllTypeSymbols());
  }
  return sym;
}

auto spp::analyse::utils::member_lookup::FindNsSymbolOrError(
  Scope const &scope,
  IdentifierAst const &ns)
  -> NamespaceSymbol* {
  auto *const sym = scope.FindNsSymbol(&ns);
  if (sym == nullptr) { RaiseMissingIdentifierAndClosestOptions(ns, {}, scope.GetAllNsSymbols()); }
  return sym;
}

auto spp::analyse::utils::member_lookup::MemberReachableBy(
  VariableSymbol const &sym, const MemberAccessForm form) -> bool {
  // A method is reached either way: how it is called (static
  // or runtime) is what decides whether it errors. Anything
  // with no runtime storage of its own is reached statically,
  // and the rest - attributes - at runtime.
  if (sym.Kind == VariableKind::FnMock) { return true; }
  return sym.IsCompTime() == (form == MemberAccessForm::Static);
}

auto spp::analyse::utils::member_lookup::MembersReachableBy(
  Vec<DeclaringVarScope> const &candidates,
  const MemberAccessForm form) -> Vec<DeclaringVarScope> {
  auto out = Vec<DeclaringVarScope>();
  for (auto const &candidate : candidates) {
    if (MemberReachableBy(*candidate.Symbol, form)) { out.emplace_back(candidate); }
  }
  return out;
}

auto spp::analyse::utils::member_lookup::MemberOf(
  Scope &type_scope, IdentifierAst const &name,
  const MemberAccessForm form) -> VariableSymbol* {
  // The cheap lookup first, which answers most accesses; only a declaration of the other form there means walking the
  // super scopes for one this form reaches.
  const auto found = type_scope.FindVarSymbol(&name, true);
  if (found == nullptr or MemberReachableBy(*found, form)) { return found; }
  const auto closest = ClosestMembers(type_scope, name, form);
  return closest.empty() ? found : closest[0].Symbol;
}

auto spp::analyse::utils::member_lookup::ClosestMembers(
  Scope &type_scope, IdentifierAst const &name,
  const MemberAccessForm form) -> Vec<DeclaringVarScope> {
  return ClosestScopes(MembersReachableBy(ScopesDeclaringVar(type_scope, name), form));
}

auto spp::analyse::utils::member_lookup::ScopesDeclaringVar(
  Scope &type_scope, IdentifierAst const &name) -> Vec<DeclaringVarScope> {
  // Each scope asked for its own symbols only: the walk already visits every super scope, so a lookup that searched
  // them too would list a super scope's member again, against the scope that asked.
  return ScopesDeclaring<DeclaringVarScope>(type_scope, name, [](Scope const &scope, IdentifierAst const &n) {
    return scope.FindVarSymbol(&n, true, false);
  });
}

auto spp::analyse::utils::member_lookup::ScopesDeclaringType(
  Scope &type_scope, TypeIdentifierAst const &name) -> Vec<DeclaringTypeScope> {
  return ScopesDeclaring<DeclaringTypeScope>(type_scope, name, [](Scope const &scope, TypeIdentifierAst const &n) {
    return scope.FindTypeSymbol(&n, true, false);
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
  Ast const &access) -> void {
  // Wrap the implementation function to raise an error
  // if there are more than 1 scopes at the equal minimum
  // level => ambiguous lookup.
  RaiseIfAmbiguousImpl(closest, access);
}

auto spp::analyse::utils::member_lookup::RaiseIfAmbiguous(
  Vec<DeclaringTypeScope> const &closest,
  Ast const &access) -> void {
  // Wrap the implementation function to raise an error
  // if there are more than 1 scopes at the equal minimum
  // level => ambiguous lookup.
  RaiseIfAmbiguousImpl(closest, access);
}

auto spp::analyse::utils::member_lookup::CheckMethodVisible(
  Scope &type_scope, IdentifierAst const &name,
  ScopeManager const &sm, CompilerMetaData const &meta) -> VariableSymbol* {
  const auto methods = ScopesDeclaringVar(type_scope, name)
    | genex::views::filter([](auto const &x) { return x.Symbol->Kind == scopes::VariableKind::FnMock; })
    | genex::to<Vec>();
  if (methods.empty()) { return nullptr; }
  auto const &cls_scope = *type_scope.NonGnScope;
  const auto any_visible = genex::any_of(methods, [&](auto const &x) {
    return visibility_utils::IsTypeMemberVisible(*x.Symbol, cls_scope, sm, meta);
  });
  if (not any_visible) { visibility_utils::CheckTypeMemberVisibility(*methods.back().Symbol, name, cls_scope, sm, meta); }
  return methods.back().Symbol;
}

auto spp::analyse::utils::member_lookup::CheckClosestMember(
  Vec<DeclaringVarScope> const &closest, IdentifierAst const &name,
  ScopeManager const &sm, CompilerMetaData const &meta) -> VariableSymbol* {
  if (closest.empty()) { return nullptr; }
  auto const &declared_in = *closest[0].Where->NonGnScope;
  auto *const declared = declared_in.FindVarSymbol(&name, true);
  if (declared != nullptr) { visibility_utils::CheckTypeMemberVisibility(*declared, name, declared_in, sm, meta); }
  else { visibility_utils::CheckTypeMemberVisibility(*closest[0].Symbol, name, *closest[0].Where, sm, meta); }
  RaiseIfAmbiguous(closest, name);
  return declared != nullptr ? declared : closest[0].Symbol;
}

auto spp::analyse::utils::member_lookup::RaiseMemberNotFound(
  TypeSymbol const &owner, TypeRef const &owner_ref, IdentifierAst const &name,
  const MemberAccessForm form, Scope const &scope) -> void {
  auto candidates = owner.LinkedScope->GetAllVarSymbols(true, true);
  const auto [fwd_ref, fwd_mut] = marker_sups::FindFwdSups(owner_ref, scope);
  for (auto const &fwd : {fwd_ref, fwd_mut}) {
    if (auto const *const target = marker_sups::FwdTargetOf(fwd).Symbol; target != nullptr and target->LinkedScope != nullptr) {
      candidates.append_range(target->LinkedScope->GetAllVarSymbols(true, true));
    }
  }
  RaiseMissingIdentifierAndClosestOptions(
    name, candidates | genex::views::filter([form](auto const *sym) { return MemberReachableBy(*sym, form); })
      | genex::to<Vec>(), {});
  std::unreachable();
}
