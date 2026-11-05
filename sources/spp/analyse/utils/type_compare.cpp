module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_compare;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
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
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
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

    /// [CHECKED]
    /// Do a variant check - this checks if the type fits in the
    /// variant, either as a single element of it, or a subset
    /// variant.
    auto VariantAccepts(
      TypeRef const &variant, TypeRef const &type, Scope const &variant_scope, Scope const &type_scope) -> bool {
      // Get all the members of the variant as a vector of type
      // references; if empty, then return false (shortcut guard).
      const auto variant_members = type_members::VariantMemberRefs(variant, variant_scope);
      if (variant_members.empty()) { return false; }

      // Get the members of the type (assuming it is a subset
      // variant). A borrow of a variant ("&Opt[S32]") is one
      // member, not a subset: its members are behind the pointer.
      const auto subset_members = type.IsBorrowed() and not variant.IsBorrowed()
        ? Vec<TypeRef>()
        : type_members::VariantMemberRefs(type, type_scope);
      if (not subset_members.empty()) {
        return genex::all_of(subset_members, [&](auto const &t) {
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
      candidates.append_range(type_members::SuperClsRefs(arg, arg_scope));
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
      // A closure borrowed as a function type ("&c" for a
      // "&FunMut[..]" parameter) is that type's fat pointer
      // already, so its superimposed types are compared with the
      // target's convention dropped: the caller has matched the
      // conventions. A named function has no storage to borrow,
      // so it keeps the convention (and fails a borrowed target).
      const auto target = mock.Symbol->Kind == scopes::TypeKind::ClosureMock
        ? func.WithoutConvention()
        : func;

      // Iterate every super class of the mock type (this gets
      // the list of FunXXX overloads), and check the type
      // against the "func" type.
      for (auto const &sup : type_members::SuperClsRefs(mock, mock_scope)) {
        if (type_predicates::IsTypeFunction(sup, mock_scope)
          and AssignableCore(sup, target, mock_scope, func_scope, true)) {
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

      // A named function's mock stands for that one function: it
      // carries nothing at runtime, so whatever reads a value of
      // that type calls that function. Nothing else fits one, not
      // even another function of the same signature. (But can be
      // coerced into its "FunRef[...]" etc).
      if (t.Symbol->IsMock()
        and (t.Symbol->Kind != scopes::TypeKind::ClosureMock or v.Symbol->Kind != scopes::TypeKind::ClosureMock)) {
        return false;
      }

      // Different templates: a function mock against a function
      // type, or a forwarding type against its target.
      if (t.Symbol->Type != v.Symbol->Type) {
        // The value is a $MockType (a function named as a value),
        // so check it against the FunXXX family overload types it
        // is superimposed with.
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

    /// The arguments an identity holds: an instance's (a type
    /// pack's list spliced in as its elements), or a variant's
    /// members (positionally, in their canonical order). None for
    /// anything else.
    auto IdArgs(const TypeId id) -> Vec<TypeIdArg> {
      if (id == nullptr) { return {}; }
      auto const &head = scopes::HeaderOf(id);
      if (head.Kind == TypeKey::Tag::Inst) {
        // A type pack's list is its elements, positionally, as written ("Tup[A, Ts]" is "A", then "Ts").
        auto out = Vec<TypeIdArg>();
        for (auto const &arg : scopes::ArgsOf(head.Args)) {
          if (scopes::IsPackTypeId(arg.TypeVal)) {
            for (const auto elem : scopes::HeaderOf(arg.TypeVal).PackElements) { out.push_back({.TypeVal = elem}); }
          }
          else if (scopes::IsPackCompId(arg.CompVal)) {
            for (const auto elem : arg.CompVal->Kids) { out.push_back({.CompVal = elem}); }
          }
          else { out.push_back(arg); }
        }
        return out;
      }
      auto members = Vec<TypeIdArg>();
      if (head.Kind == TypeKey::Tag::Variant) {
        for (const auto m : head.VariantMembers) { members.push_back({.TypeVal = m}); }
      }
      return members;
    }

    /// Whether an identity's argument is a variadic parameter
    /// nothing has bound where it was read: a pack, which stands
    /// for any number of arguments.
    auto IsUnboundPackArg(TypeIdArg const &arg) -> bool {
      if (arg.TypeVal != nullptr) {
        auto const &head = scopes::HeaderOf(arg.TypeVal);
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
      const auto is_variadic = scopes::HeaderOf(target.Id).Kind == Tag::Variant
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
  const auto type_sym = concrete.Symbol;
  if (type_sym == nullptr) { return nullptr; }

  // A hidden pack's elements are the type it derives from ("packs::DeclareHiddenPack": "..xs: S32"'s elements are
  // "S32"s), so it meets what that type meets.
  if (type_sym->IsGn() and type_sym->IsVariadic and type_sym->DerivesFromSymbol != nullptr) {
    auto &derived = *type_sym->DerivesFromSymbol;
    return UnmetConstraint(
      constraints, TypeRef::Of(derived, concrete_scope), is_self,
      constraints_owner_scope, concrete_scope);
  }

  // The concrete type and each class it is superimposed as,
  // resolved where each is read.
  auto sup_info = Vec<Pair<TypeRef, Scope const*>>{};
  if (is_self and not type_sym->IsGn() and type_sym->LinkedScope != nullptr) {
    sup_info.emplace_back(concrete.WithoutConvention().ReadIn(*type_sym->LinkedScope), type_sym->LinkedScope);
  }

  // The bound type itself, by value (a binding's borrow is no
  // part of what its constraints are checked against), then each
  // class it is superimposed as: its sup scopes, or for a generic
  // type symbol the classes its own constraints name.
  sup_info.emplace_back(concrete.WithoutConvention(), &concrete_scope);

  // For a Non-generic type, read the constraints as the sup
  // scopes of the scope representing the type.
  if (type_sym->LinkedScope != nullptr) {
    sup_info.append_range(type_members::SuperClsReads(type_sym->LinkedScope->GetSupScopes()));
  }

  // Otherwise, pull the constraints off of the type symbol's
  // generic constraints vector: the classes they name.
  else {
    auto constraint_scopes = Vec<Scope*>();
    for (auto const &constraint : type_sym->TypeConstraints) {
      const auto constraint_sym = constraints_owner_scope.FindTypeSymbol(constraint.get());
      if (constraint_sym != nullptr and constraint_sym->LinkedScope != nullptr) {
        constraint_scopes.emplace_back(constraint_sym->LinkedScope);
      }
    }
    sup_info.append_range(type_members::SuperClsReads(constraint_scopes));
  }

  // Compare each constraint against the concrete type and its
  // supertypes.
  for (auto const &constraint : constraints) {
    // Prevent a non-thread-safe type from being used when
    // thread safety is required. Instead of auto injecting
    // as this can blow up generic instantiations.
    if (constraint->LastTypePart()->Name == THREAD_SAFE->LastTypePart()->Name
      and TypeRef::ForKindCheck(*constraint, constraints_owner_scope).IsA(*THREAD_SAFE, constraints_owner_scope)) {
      if (type_sym->IsThreadSafe()) { continue; }
      return constraint.get();
    }

    // If any constraint is not met, return it so the caller
    // can decide whether to raise an error.
    const auto wanted = TypeRef::Of(*constraint, constraints_owner_scope);
    const auto matched = genex::any_of(sup_info, [&](auto const &sup) {
      return Assignable(wanted, sup.first, constraints_owner_scope, *sup.second);
    });

    // "Self" in a constraint and in a superimposed class is the type checked: "S32 ext Add" is "Add[Rhs=Self,
    // Ret=Self]" in its sup scope, which "Add[S32]" ("Add[Rhs=S32, Ret=Self]") only meets once both are read with
    // "Self" as "S32". Compared by identity, as the instance the two name need not exist. Tried only when nothing
    // matched as written, so a match costs no substitution.
    // Only a superimposed class of the constraint's own template can match it ("Add" against "Add"), so only those
    // are substituted: each substitution interns a key.
    if (matched) { continue; }
    const auto self = concrete.WithoutConvention().Id;
    const auto wanted_head = wanted.Id != nullptr ? scopes::HeaderOf(wanted.Id).Ptr : nullptr;
    const auto same_template = [&](auto const &sup) {
      return sup.first.Id != nullptr and wanted_head != nullptr and scopes::HeaderOf(sup.first.Id).Ptr == wanted_head;
    };
    if (self == nullptr or not genex::any_of(sup_info, same_template)) { return constraint.get(); }
    const auto wanted_as_self = wanted.Id->HasSelf ? scopes::SubstituteSelfId(wanted.Id, self) : wanted.Id;
    const auto matched_as_self = genex::any_of(sup_info, [&](auto const &sup) {
      return same_template(sup) and scopes::SubstituteSelfId(sup.first.Id, self) == wanted_as_self;
    });
    if (not matched_as_self) { return constraint.get(); }
  }

  // All constraints are satisfied.
  return nullptr;
}

auto spp::analyse::utils::type_compare::IsOpenTuple(
  TypeRef const &type, Scope const &scope) -> bool {
  // Read off the identity, not the spelling: inside an instance a
  // caller's pack can be bound to a parameter of another name.
  if (type.Symbol == nullptr or not type_predicates::IsTypeTuple(type, scope)) { return false; }
  const auto args = IdArgs(type.Id);
  return not args.empty() and IsUnboundPackArg(args.back());
}

auto spp::analyse::utils::type_compare::OpenTupleElement(
  TypeRef const &type, const std::size_t n, Scope const &scope) -> TypeRef {
  // The elements before the pack are written one by one; every
  // element from the pack on is one of the pack's.
  const auto args = IdArgs(type.Id);
  auto const &arg = n + 1 < args.size() ? args[n] : args.back();
  const auto element = TypeRef::Of(arg.TypeVal, scope);

  // A hidden pack's elements are the type it derives from ("packs::DeclareHiddenPack"): a fold's results over
  // "..xs: S32" are "S32"s, and are used as them.
  auto const *const sym = element.Symbol;
  if (sym != nullptr and sym->IsGn() and sym->IsVariadic and sym->DerivesFromSymbol != nullptr) {
    return TypeRef::Of(*sym->DerivesFromSymbol, scope);
  }
  return element;
}
