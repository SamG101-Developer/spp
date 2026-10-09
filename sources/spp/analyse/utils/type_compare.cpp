module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_compare;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::type_compare {
  namespace {
    /// [CHECKED]
    /// 2 conventions are only equal if they are the same, or if
    /// we are coalescing &mut into &
    auto ConventionTagEq(
      const ConventionTag lhs,
      const ConventionTag rhs)
      -> bool {
      // Guard against an auto-match for true, and anything that
      // involves only 1 borrow for false. Coalesce "&mut" -> "&".
      if (lhs == rhs) { return true; }
      if (lhs == ConventionTag::MOV or rhs == ConventionTag::MOV) { return false; }
      return not(lhs == ConventionTag::MUT and rhs == ConventionTag::REF);
    }

    /// [CHECKED]
    /// If the type has a convention, then get its tag; otherwise,
    /// use the "MOV" convention (the default no-token convention).
    auto ConventionTagOf(
      TypeAst const &type) -> ConventionTag {
      // Extract the convention, and do a nullptr -> MOV mapping
      // if it doesn't exist.
      const auto conv = type.GetConvention();
      return conv != nullptr ? conv->Tag() : ConventionTag::MOV;
    }

    auto AssignableCore(
      TypeRef const &target, TypeRef const &value, Scope const &target_scope, Scope const &value_scope,
      bool widen_variant) -> bool;

    auto SameType(
      TypeRef const &lhs, TypeRef const &rhs, Scope const &lhs_scope, Scope const &rhs_scope) -> bool;

    /// A resolved type named as its symbol names it, under its
    /// convention: how a variant's members are listed.
    auto AsType(
      TypeRef const &ref)
      -> Shared<TypeAst> {
      auto name = ref.Symbol->FqName();
      return ref.Conv != ConventionTag::MOV ? name->WithConvention(ConventionAstOf(ref.Conv)) : name;
    }

    /// For a variant type, flatten any sub-variants into the
    /// single variant type list, and remove any duplicates too
    /// (flattening can introduce duplicates, or they might be
    /// pre-existing).
    auto FlattenVariants(
      TypeAst const &variants, Scope const &scope) -> Vec<Pair<Shared<TypeAst>, TypeRef>> {
      // Create the list of types that will be the final returned
      // flattened, deduplicated list.
      auto out = Vec<Pair<Shared<TypeAst>, TypeRef>>();

      // The adder function first checks that the type doesn't
      // exist within the output list, before adding it in.
      const auto add_unique = [&](Shared<TypeAst> const &member, TypeRef const &ref) {
        if (genex::any_of(out, [&](auto const &x) { return SameType(ref, x.second, scope, scope); })) { return; }
        out.EmplaceBack(member, ref);
      };

      // Get the variant member's types from the tuple that is
      // the "..Variants" generic parameter's argument. Add or
      // flatten each type into the resulting list.
      for (const auto arg : variants.LastTypePart()->GnArgGroup->GetTypeArgs()) {
        const auto ref = TypeRef::Of(*arg->TypeVal, scope);
        const auto nested = VariantMemberRefs(ref, scope);
        if (nested.IsEmpty()) { add_unique(arg->TypeVal, ref); }
        else { for (auto const &m : nested) { add_unique(AsType(m), m); } }
      }

      // Return the new filled flat vector.
      return out;
    }

    /// [CHECKED]
    /// Do a variant check - this checks if the type fits in the
    /// variant, either as a single element of it, or a subset
    /// variant.
    auto VariantAccepts(
      TypeRef const &variant, TypeRef const &type, Scope const &variant_scope, Scope const &type_scope) -> bool {
      // Get all the members of the variant as a vector of type
      // references; if empty, then return false (shortcut guard).
      const auto variant_members = VariantMemberRefs(variant, variant_scope);
      if (variant_members.IsEmpty()) { return false; }

      // Get the members of the type (assuming it is a subset
      // variant.
      const auto type_members = VariantMemberRefs(type, type_scope);
      if (not type_members.IsEmpty()) {
        return genex::all_of(type_members, [&](auto const &t) {
          return genex::any_of(variant_members, [&](auto const &v) {
            return SameType(v, t, variant_scope, type_scope);
          });
        });
      }

      // Otherwise, the "type" is a single type, so just check
      // that it is can be assigned into one of the variant members.
      return genex::any_of(variant_members, [&](auto const &v) {
        return AssignableCore(v, type, variant_scope, type_scope, true);
      });
    }

    /// [CHECKED]
    /// The type forwarding matcher. This allows "&Str" to go
    /// into an "&StrView", because a forwarding extension has
    /// been defined on "Str", targeting "&StrView". Convention
    /// must match.
    auto ForwardsTo(
      TypeRef const &arg, TypeRef const &param, Scope const &arg_scope, Scope const &param_scope) -> bool {
      using generate::common_types_precompiled::FWD_REF;
      using generate::common_types_precompiled::FWD_MUT;

      // Do a convention match. "Str" cannot go to "&StrView, but
      // "&Str" can. Same for mutable borrows.
      const auto both_ref = arg.Conv == ConventionTag::REF and param.Conv == ConventionTag::REF;
      const auto both_mut = arg.Conv == ConventionTag::MUT and param.Conv == ConventionTag::MUT;
      if (not both_ref and not both_mut) { return false; }

      // There are no forwarding extensions definable over the
      // generic, so this will always be false.
      // Todo: Enforce this ^^^ (currently can be done).
      if (arg.Symbol == nullptr or arg.Symbol->IsGn() or arg.Symbol->LinkedScope == nullptr) { return false; }

      // An argument already naming the parameter's own class is
      // not forwarded to it (see "TypeFwdEq").
      if (param.Symbol != nullptr and
        arg.Symbol->LinkedScope->NonGnScope == (
          param.Symbol->LinkedScope != nullptr ? param.Symbol->LinkedScope->NonGnScope : nullptr)) {
        return false;
      }

      // Check each super type on the type for a FwdRef or FwdMut
      // superimposition extension and select it.
      // Todo: Move to "marker_sups" util package?
      auto const &fwd_target = both_ref ? *FWD_REF : *FWD_MUT;
      auto candidates = Vec{arg};
      candidates.AppendRange(type_members::SuperClsRefs(arg, arg_scope));
      for (auto const &candidate : candidates) {
        if (candidate.Symbol == nullptr or not candidate.IsA(fwd_target, arg_scope)) { continue; }
        auto inner = candidate.Symbol->TypeArgRef("T");
        if (inner.Symbol == nullptr) { continue; }
        inner.Conv = param.Conv;
        if (AssignableCore(inner, param, param_scope, param_scope, true)) { return true; }
      }
      return false;
    }

    /// [CHECKED]
    /// Determine if a $ mock type's overload function types
    /// include the "func" type. The "func" type will be one of
    /// the 3 FunXXX types, generically instantiated.
    auto MockMatches(
      TypeRef const &mock, TypeRef const &func, Scope const &mock_scope, Scope const &func_scope) -> bool {
      // Iterate every super class of the mock type (this gets
      // the list of FunXXX overloads), and check the type
      // against the "func" type.
      for (auto const &sup : type_members::SuperClsRefs(mock, mock_scope)) {
        if (type_predicates::IsTypeFunction(sup, mock_scope)
          and AssignableCore(sup, func, mock_scope, func_scope, true)) {
          return true;
        }
      }
      return fn_values::MatchFnValue(mock, func, func_scope).has_value();
    }

    /// [CHECKED]
    /// If either type is "Self", the class it stands for where
    /// the other side is read (a binding's bound type when the
    /// other side is a binding), read where its own side is;
    /// otherwise the types as they are. Both are none when a
    /// "Self" stands for nothing there. "both_self" is set when
    /// both are "Self", which is all a comparison needs.
    auto ReadSelfPair(
      TypeRef const &lhs, TypeRef const &rhs, Scope const &lhs_scope, Scope const &rhs_scope, bool &both_self)
      -> Pair<TypeRef, TypeRef> {
      //
      const auto lhs_self = lhs.Symbol->IsSelf();
      const auto rhs_self = rhs.Symbol->IsSelf();
      both_self = lhs_self and rhs_self;
      if (not lhs_self and not rhs_self) { return {lhs, rhs}; }

      //
      auto lhs_sym = lhs_self ? rhs_scope.FindSelfSymbol() : lhs.Symbol;
      auto rhs_sym = rhs_self ? lhs_scope.FindSelfSymbol() : rhs.Symbol;
      if (lhs_sym == nullptr or rhs_sym == nullptr) { return {}; }
      if (lhs_self and rhs_sym->Type != nullptr) { lhs_sym = lhs_sym->AsBound(); }
      if (rhs_self and lhs_sym->Type != nullptr) { rhs_sym = rhs_sym->AsBound(); }
      return {
        lhs_self ? TypeRef::Of(*lhs_sym, lhs_scope, lhs.Conv, TypeRef::OnMissing::Open) : lhs,
        rhs_self ? TypeRef::Of(*rhs_sym, rhs_scope, rhs.Conv, TypeRef::OnMissing::Open) : rhs
      };
    }

    /// [FWD]
    auto AssignableArgs(
      TypeRef const &target, TypeRef const &value, Scope const &target_scope, Scope const &value_scope) -> bool;

    /// [CHECKED]
    /// Check to type identity, shortcutting on never types,
    /// then a nullptr and convention check. Finally we do a
    /// "Self"-pair resolution/comparison, before comparing
    /// the identities.
    auto SameType(
      TypeRef const &lhs, TypeRef const &rhs, Scope const &lhs_scope, Scope const &rhs_scope) -> bool {
      // Never and convention shortcut guards to avoid unnecessary
      // symbol lookups.
      if (lhs.IsNever or rhs.IsNever) { return lhs.IsNever and rhs.IsNever; }
      if (lhs.Symbol == nullptr or rhs.Symbol == nullptr or lhs.Conv != rhs.Conv) { return false; }

      // Do the "Self" resolution for the two types, and if they
      // are both "Self" types, return true.
      auto both_self = false;
      const auto [l, r] = ReadSelfPair(lhs, rhs, lhs_scope, rhs_scope, both_self);
      return both_self or l.SameAs(r);
    }

    /// [CHECKED]
    /// There are a number of tests that allow one type to be
    /// assignable into another, detailed in the .ixx file.
    auto AssignableCore(
      TypeRef const &target, TypeRef const &value, Scope const &target_scope, Scope const &value_scope,
      const bool widen_variant) -> bool {
      // Shortcut guards - if the value is the never type, it
      // fits anywhere; if it's the target, nothing can match
      // it (except never, which is pre-checked).
      if (value.IsNever) { return true; }
      if (target.IsNever) { return false; }
      if (target.Symbol == nullptr or value.Symbol == nullptr) { return false; }

      // Run the "Self" check; if both types are "Self" then
      // return true.
      auto both_self = false;
      const auto [t, v] = ReadSelfPair(target, value, target_scope, value_scope, both_self);
      if (both_self) { return true; }
      if (t.Symbol == nullptr) { return false; }

      // If we are allowing variant checks, then check the value
      // can be accepted by the target variant; return early.
      if (widen_variant and VariantAccepts(target, value, target_scope, value_scope)) {
        return true;
      }

      // Check the conventions are compatible. A mismatch here,
      // aside from permitted coalescing, is an early return false.
      if (not ConventionTagEq(target.Conv, value.Conv)) { return false; }

      // Next compare the identities of the types from the "Self"
      // check, the conventions having been compared above.
      if (t.Id != nullptr and t.Id == v.Id) { return true; }

      // Different templates: a function mock against a function
      // type, or a forwarding type against its target.
      if (t.Symbol->Type != v.Symbol->Type) {
        // The target is a $MockType, so check it against FunXXX
        // family overload types.
        if (t.Symbol->IsMock()) { return MockMatches(t, value, target_scope, value_scope); }

        // Same as above but the other way around. Todo: Are both
        // ways needed?
        if (v.Symbol->IsMock()) { return MockMatches(v, target, value_scope, target_scope); }

        // Otherwise, consider a forwarding check, of "Vec" to the
        // "&View" type for example.
        return ForwardsTo(v, t, value_scope, target_scope);
      }

      // If we reach this point, then the templates are the same,
      // but the ids are not, and therefore we need to consider
      // the generic arguments.
      return AssignableArgs(t, v, target_scope, value_scope);
    }

    /// The arguments an identity holds: an instance's, or a
    /// variant's members (positionally, as its "Variants" tuple
    /// lists them, in their canonical order). None for anything
    /// else.
    auto IdArgs(TypeId id) -> std::vector<TypeIdArg> {
      if (id == nullptr) { return {}; }
      auto const &head = scopes::HeadOf(id);
      if (head.Kind == TypeKey::Tag::Inst) { return scopes::ArgsOf(head.Args); }
      auto members = std::vector<TypeIdArg>();
      if (head.Kind == TypeKey::Tag::Variant) {
        for (const auto m : head.Members) { members.push_back({.TypeVal = m}); }
      }
      return members;
    }

    /// Whether an identity's argument is a variadic parameter
    /// nothing has bound where it was read: a pack, which stands
    /// for any number of arguments.
    auto IsUnboundPackArg(TypeIdArg const &arg) -> bool {
      if (arg.TypeVal != nullptr) {
        auto const &head = scopes::HeadOf(arg.TypeVal);
        auto const *const param = head.Kind == TypeKey::Tag::TypeParam
          ? FindGnTypeParamById(head.TypeParamId)
          : nullptr;
        return param != nullptr and param->IsVariadic;
      }
      auto const *const node = arg.CompVal;
      auto const *const param = node != nullptr and node->Kind == CompKey::Part::Param
        ? FindGnCompParamById(node->ParamId)
        : nullptr;
      return param != nullptr and param->IsVariadic;
    }

    /// [CHECKED]
    /// Run the assignable core for pairs of generic arguments,
    /// read off the two identities, but also consider the
    /// variadic case: a trailing unbound pack on the value
    /// absorbs what is left.
    auto AssignableArgs(
      TypeRef const &target, TypeRef const &value, Scope const &target_scope, Scope const &value_scope) -> bool {
      using Tag = TypeKey::Tag;
      const auto target_args = IdArgs(target.Id);
      const auto value_args = IdArgs(value.Id);

      // Without a pack, a variadic template's (or a variant's)
      // two lists have to be the same length, or the shorter
      // would be read as a prefix of the longer. A fixed
      // template needs no check: its parameters fix the length.
      const auto has_pack = not value_args.empty() and IsUnboundPackArg(value_args.back());
      const auto fixed_len = has_pack ? value_args.size() - 1 : std::min(target_args.size(), value_args.size());
      const auto is_variadic = scopes::HeadOf(target.Id).Kind == Tag::Variant
        or (target.Symbol->Type != nullptr and target.Symbol->Type->GnParamGroup->GetVariadicParam() != nullptr);
      if (has_pack ? target_args.size() < fixed_len : is_variadic and target_args.size() != value_args.size()) {
        return false;
      }

      // Every argument has to match: types by assignability
      // ("Self" matching "Self"), comp values by identity.
      for (auto i = 0uz; i < fixed_len; ++i) {
        auto const &t = target_args[i];
        auto const &v = value_args[i];
        const auto ok = t.TypeVal != nullptr and v.TypeVal != nullptr
          ? (IsSelfTypeId(t.TypeVal) and IsSelfTypeId(v.TypeVal))
          or AssignableCore(
            TypeRef::Of(t.TypeVal, target_scope), TypeRef::Of(v.TypeVal, value_scope), target_scope, value_scope, true)
          : t.TypeVal == nullptr and v.TypeVal == nullptr and t.CompVal == v.CompVal;
        if (not ok) { return false; }
      }
      return true;
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::ConventionEq(
  TypeAst const &lhs_type,
  TypeAst const &rhs_type)
  -> bool {
  // The convention equality checker runs a tag equality
  // on the extracted tags. Nullptr-safe.
  return ConventionTagEq(ConventionTagOf(lhs_type), ConventionTagOf(rhs_type));
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::TypeEq(
  TypeRef const &lhs, TypeRef const &rhs,
  Scope const &lhs_scope, Scope const &rhs_scope) -> bool {
  // Route through the internal "same type" check.
  return SameType(lhs, rhs, lhs_scope, rhs_scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::Assignable(
  TypeRef const &target, TypeRef const &value,
  Scope const &target_scope, Scope const &value_scope) -> bool {
  // Run through the core assignment checker.
  return AssignableCore(target, value, target_scope, value_scope, true);
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::TypeFwdEq(
  TypeRef const &arg, TypeRef const &param,
  Scope const &arg_scope, Scope const &param_scope) -> bool {
  // Wrap the internal forwarding call checker.
  return ForwardsTo(arg, param, arg_scope, param_scope);
}

auto spp::analyse::utils::type_compare::UnmetConstraint(
  Vec<Shared<TypeAst>> const &constraints, TypeRef const &concrete, const bool is_self,
  Scope const &constraints_owner_scope, Scope const &concrete_scope) -> TypeAst const* {
  //
  using generate::common_types_precompiled::THREAD_SAFE;
  auto type_sym = concrete.Symbol;
  if (type_sym == nullptr) { return nullptr; }

  // The concrete type and each class it is superimposed as,
  // resolved where each is read.
  auto sup_info = Vec<Pair<TypeRef, Scope const*>>{};
  if (is_self and not type_sym->IsGn() and type_sym->LinkedScope != nullptr) {
    sup_info.EmplaceBack(concrete.WithoutConvention().ReadIn(*type_sym->LinkedScope), type_sym->LinkedScope);
  }

  // Get all the sup scopes of the concrete type, which we
  // compare against required constraints. For generic type
  // symbols, that generic's constraints are stored on the
  // symbol. Todo: Condense this block.
  const auto sup_scopes = type_sym->LinkedScope
    ? type_sym->LinkedScope->GetSupScopes()
    : type_sym->TypeConstraints | genex::views::transform([&](auto const &constraint) {
      return constraints_owner_scope.FindTypeSymbol(constraint.get())->LinkedScope;
    }) | genex::to<Vec>();
  // The bound type itself, by value: a binding's borrow
  // is no part of what its constraints are checked against.
  sup_info.EmplaceBack(concrete.WithoutConvention(), &concrete_scope);
  for (auto const *sup_scope : sup_scopes) {
    if (sup_scope->LinkedTypeSymbol == nullptr or AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    sup_info.EmplaceBack(TypeRef::Of(*sup_scope->LinkedTypeSymbol, *sup_scope), sup_scope);
  }

  // Compare each constraint against the concrete type and
  // its supertypes.
  for (auto const &constraint : constraints) {
    // Prevent a non-thread-safe type from being used when
    // thread safety is required. Instead of auto injecting
    // as this can blow up generic instantiations.
    if (constraint->LastTypePart()->Name == THREAD_SAFE->LastTypePart()->Name
      and constraints_owner_scope.TypeIdOf(*constraint) == constraints_owner_scope.TypeIdOf(*THREAD_SAFE)) {
      if (type_sym->IsThreadSafe()) { continue; }
      return constraint.get();
    }

    // If any constraint is not met, return it so the caller
    // can decide whether to raise an error.
    const auto wanted = TypeRef::Of(*constraint, constraints_owner_scope);
    const auto matched = genex::any_of(sup_info, [&](auto const &sup) {
      return Assignable(wanted, sup.first, constraints_owner_scope, *sup.second);
    });
    if (not matched) { return constraint.get(); }
  }

  // All constraints are satisfied.
  return nullptr;
}

auto spp::analyse::utils::type_compare::OrderVariantMembers(
  Vec<Pair<Shared<TypeAst>, TypeSymbol const*>> members)
  -> Vec<Shared<TypeAst>> {
  auto keyed = Vec<Pair<Str, Shared<TypeAst>>>();
  for (auto &[type, sym] : members) {
    keyed.EmplaceBack(sym != nullptr ? sym->FqName()->ToString() : type->ToString(), std::move(type));
  }
  genex::actions::stable_sort(keyed, {}, [](auto const &k) -> Str const& { return k.first; });
  auto out = Vec<Shared<TypeAst>>();
  for (auto &[_, type] : keyed) { out.EmplaceBack(std::move(type)); }
  return out;
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::VariantMembers(
  TypeAst const &type, Scope const &scope) -> Vec<Shared<TypeAst>> {
  // A type based overload of the variant member detection,
  // which flattens and deduplicates.
  const auto variants = type.LastTypePart()->GnArgGroup->At("Variants");
  if (variants != nullptr and variants->IsTypeArg()) {
    return FlattenVariants(*variants->TypeVal, scope)
      | genex::views::transform([](auto const &x) { return x.first; })
      | genex::to<Vec>();
  }

  return VariantMemberRefs(TypeRef::Of(type, scope), scope)
    | genex::views::transform([](auto const &x) { return AsType(x); })
    | genex::to<Vec>();
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::VariantMemberRefs(
  TypeRef const &ref, Scope const &scope) -> Vec<TypeRef> {
  // Never types won't have variant members so guard against
  // that at the start.
  const auto sym = ref.IsNever ? nullptr : ref.Symbol;
  if (sym == nullptr) { return {}; }

  // Its identity lists its members, flattened and in order,
  // each what it means; anything else is not a variant.
  if (ref.Id == nullptr or scopes::HeadOf(ref.Id).Kind != TypeKey::Tag::Variant) { return {}; }
  auto members = Vec<TypeRef>();
  // Read straight off each member's identity, one per member:
  // a member's index is its tag.
  for (const auto member : scopes::HeadOf(ref.Id).Members) { members.EmplaceBack(TypeRef::Of(member, scope)); }
  return members;
}
