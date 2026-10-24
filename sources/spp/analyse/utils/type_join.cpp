module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_join;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.type_ast;
import genex;

SPP_MOD_BEGIN
spp::analyse::utils::type_join::TypeJoin::TypeJoin(
  Shared<TypeAst> target, Scope const *target_scope, Ast const *target_site) :
  _Target{.Site = target_site, .Type = std::move(target), .TypeScope = target_scope} {
}

auto spp::analyse::utils::type_join::TypeJoin::Give(
  Ast const &site, Shared<TypeAst> type, Scope const &type_scope, const bool diverges)
  -> void {
  _Values.push_back({.Site = &site, .Type = std::move(type), .TypeScope = &type_scope, .Diverges = diverges});
}

auto spp::analyse::utils::type_join::TypeJoin::Solve() const
  -> JoinedValue {
  using errors::SppTypeMismatchError;
  const auto ref_of = [](JoinedValue const &v) { return TypeRef::Of(*v.Type, *v.TypeScope); };

  // Only a value that reaches the join takes part. When every one
  // diverges, those typed "!" stand for the whole (it fits whatever
  // it is assigned to), and failing those, all of them.
  auto valued = _Values
    | genex::views::filter([](auto const &v) { return not v.Diverges; })
    | genex::to<Vec>();
  const auto every_value_diverges = valued.empty();
  if (valued.empty()) {
    valued = _Values | genex::views::filter([](auto const &v) { return v.Type->IsNeverType(); }) | genex::to<Vec>();
  }
  if (valued.empty()) { valued = _Values; }
  if (valued.empty()) { return JoinedValue(); }

  // The target, when there is one; otherwise the widest variant
  // among the values; otherwise the first value with a type of
  // its own ("!" has none for the others to agree with).
  auto agreed = _Target;
  if (agreed.Type == nullptr) {
    auto widest = 0uz;
    for (auto const &v : valued) {
      const auto ref = ref_of(v);
      if (not type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*v.Type, *v.TypeScope), *v.TypeScope)) { continue; }
      const auto size = type_members::VariantMemberRefs(ref, *v.TypeScope).size();
      if (size > widest) {
        agreed = v;
        widest = size;
      }
    }
  }
  if (agreed.Type == nullptr) {
    const auto first = genex::find_if(valued, [](auto const &v) { return not v.Type->IsNeverType(); });
    agreed = first != valued.end() ? *first : valued.front();
  }

  // Every other value goes into it. A "!" value fits any type.
  if (every_value_diverges) {
    agreed.Diverges = true;
    return agreed;
  }
  for (auto const &v : valued) {
    if (v.Site == agreed.Site and v.Type == agreed.Type) { continue; }
    if (type_compare::Assignable(ref_of(agreed), ref_of(v), *agreed.TypeScope, *v.TypeScope)) { continue; }
    auto const &expected_at = agreed.Site != nullptr ? *agreed.Site : *v.Site;
    Raise<SppTypeMismatchError>(ERR_ARGS(expected_at, *agreed.Type, *v.Site, *v.Type));
  }
  return agreed;
}

auto spp::analyse::utils::type_join::TypeJoin::InstantiateFnValues(
  JoinedValue const &agreed, ScopeManager *sm, CompilerMetaData *meta) const
  -> void {
  if (agreed.Type == nullptr) { return; }
  const auto agreed_ref = TypeRef::Of(*agreed.Type, *agreed.TypeScope);
  for (auto const &v : _Values) {
    fn_values::InstantiateFnValue(TypeRef::Of(*v.Type, *v.TypeScope), agreed_ref, sm, meta);
  }
}

auto spp::analyse::utils::type_join::TypeJoin::Empty() const
  -> bool {
  return _Values.empty();
}

auto spp::analyse::utils::type_join::TypeJoin::Values() const
  -> Vec<JoinedValue> const& {
  return _Values;
}

SPP_MOD_END
