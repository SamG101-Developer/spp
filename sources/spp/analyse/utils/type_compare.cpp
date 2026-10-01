module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_compare;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.function_values;
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
    /// How two written generic argument lists line up, for the
    /// type they were both written for.
    struct ArgListArity {
      /// Whether the two lengths can describe the same type at
      /// all.
      bool Compatible;

      /// How many leading arguments are compared one against
      /// one; anything past this is the pack.
      std::size_t FixedLen;

      /// The right-hand-side's trailing argument, when it names a
      /// pack (of either kind) that takes what is left.
      GenericArgumentAst const *Pack;
    };

    /// [CHECKED]
    /// The generic arguments on the type on a type symbol.
    /// Handles type aliases correctly too.
    auto SymArgGroup(
      TypeSymbol const &sym)-> GenericArgumentGroupAst const& {
      // If the type symbol holds an alias type, get the generics
      // on an alias's resolved type.
      if (sym.Alias != nullptr and sym.Alias->Resolved != nullptr) {
        return *sym.Alias->Resolved->LastTypePart()->GnArgGroup;
      }

      // Otherwise, get the argument group on the type symbol's
      // type name.
      return *sym.Name->GnArgGroup;
    }

    /** Whether a resolved type satisfies every constraint ("UnmetConstraint"); true when there are none. */
    auto ConstraintsHold(
      Vec<Shared<TypeAst>> const &constraints,
      TypeRef const &type,
      Scope const &constraint_scope,
      Scope const &type_scope)
      -> bool {
      return constraints.IsEmpty() or type.Sym == nullptr
        or UnmetConstraint(constraints, *type.Sym, false, constraint_scope, type_scope) == nullptr;
    }

    /**
     * A type's generic arguments as the instantiation it stands for holds them. An alias is written with its own
     * arguments ("SizedIntegerUnsigned[w]") but stands for an instantiation of what it names, which holds all of that
     * class's ("SizedInteger[w=w, signed=false]"); lining the written lists up would leave the extra ones unmatched -
     * and, when inferring, unbound. Anything else keeps its written arguments.
     * @param type The type whose arguments are wanted.
     * @param scope The scope @p type resolves in.
     */
    auto InstanceArgs(
      TypeAst const &type,
      Scope const &scope)
      -> Vec<Unique<GenericArgumentAst>> const& {
      // A name written through an alias ("Res[T, E]") stands for what the alias resolves to, whose arguments are its
      // class's ("Var[Variants=..]").
      auto const *const head = scope.GetTypeSymbol(type.WithoutGenerics()->WithoutConvention().get());
      if (head != nullptr and head->Alias != nullptr) {
        if (auto const *const sym = TypeRef::Of(type, scope).Sym; sym != nullptr) { return SymArgGroup(*sym).Args; }
      }
      return type.LastTypePart()->GnArgGroup->Args;
    }

    /// Determine the compatibility/arity of two generic argument
    /// groups. They are always on the same template type, so pass
    /// that in with a scope. A trailing pack of either kind takes
    /// what is left.
    auto MatchArgListArity(
      ClassPrototypeAst const *proto, Vec<Unique<GenericArgumentAst>> const &lhs_args,
      Vec<Unique<GenericArgumentAst>> const &rhs_args, Scope const &rhs_scope) -> ArgListArity {
      using namespace spp::asts;

      // The trailing argument is a pack only when it names a
      // variadic parameter that is still unbound.
      auto const *const pack = not rhs_args.IsEmpty() and packs::IsUnboundPackNamed(*rhs_args.Back(), rhs_scope)
        ? rhs_args.Back().get()
        : nullptr;

      // A pack absorbs the remainder, so the only requirement
      // is that the arguments it does not cover are all there.
      if (pack != nullptr) {
        const auto fixed_len = rhs_args.Len() - 1;
        return {
          .Compatible = lhs_args.Len() >= fixed_len,
          .FixedLen = std::min(fixed_len, lhs_args.Len()),
          .Pack = pack
        };
      }

      // Without one, a variadic prototype's two lists have to
      // be the same length, or the shorter would be read as a
      // prefix of the longer. A fixed prototype needs no check:
      // its parameters already fix the length.
      const auto is_variadic = proto != nullptr and proto->GnParamGroup->GetVariadicParams() != nullptr;
      return {
        .Compatible = not is_variadic or lhs_args.Len() == rhs_args.Len(),
        .FixedLen = std::min(lhs_args.Len(), rhs_args.Len()),
        .Pack = nullptr
      };
    }

    /**
     * Whether every element a pack swallowed satisfies the pack's own constraints. A variadic parameter constrains each
     * of the types it stands for rather than the list as a whole, so "sup [..T: Copy] Tup[T]" attaches to a tuple only
     * when every one of its elements is copyable.
     *
     * @param pack The variadic parameter's symbol, which carries the constraints.
     * @param lhs_args The written arguments the pack was matched against.
     * @param fixed_len How many of those are covered one for one, and so are not part of the pack.
     * @param pack_scope The scope the constraints are named in.
     * @param arg_scope The scope the arguments are named in.
     */
    auto PackConstraintsSatisfied(
      GenericArgumentAst const &pack_arg,
      Vec<Unique<GenericArgumentAst>> const &lhs_args,
      const std::size_t fixed_len,
      Scope const &pack_scope,
      Scope const &arg_scope)
      -> bool {
      // A comp pack's values are checked against its type where they are bound ("_CheckCompArgTypes"), as a single
      // comp argument's are, so only a type pack has anything to check here.
      if (pack_arg.TypeVal == nullptr) { return true; }
      auto const *const pack = pack_scope.GetTypeSymbol(pack_arg.TypeVal->WithoutGenerics().get(), false);
      if (pack == nullptr or pack->GenericConstraints.IsEmpty()) { return true; }

      for (auto i = fixed_len; i < lhs_args.Len(); ++i) {
        if (lhs_args[i]->TypeVal == nullptr) { return false; }
        if (not ConstraintsHold(
          pack->GenericConstraints, TypeRef::Of(*lhs_args[i]->TypeVal, arg_scope), pack_scope, arg_scope)) {
          return false;
        }
      }
      return true;
    }
  }
}

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

    /** A resolved type named as its symbol names it, under its convention: how a variant's members are listed. */
    auto AsType(
      TypeRef const &ref)
      -> Shared<TypeAst> {
      auto name = ref.Sym->FqName();
      if (ref.Conv == ConventionTag::MUT) {
        return name->WithConvention(MakeUnique<ConventionMutAst>(nullptr, nullptr));
      }
      if (ref.Conv == ConventionTag::REF) { return name->WithConvention(MakeUnique<ConventionRefAst>(nullptr)); }
      return name;
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
        const auto nested = VariantMembers(ref, scope);
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
      const auto variant_members = VariantMembers(variant, variant_scope);
      if (variant_members.IsEmpty()) { return false; }

      // Get the members of the type (assuming it is a subset
      // variant.
      const auto type_members = VariantMembers(type, type_scope);
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
      if (arg.Sym == nullptr or arg.Sym->IsTypeGeneric() or arg.Sym->LinkedScope == nullptr) { return false; }

      // An argument already naming the parameter's own class is
      // not forwarded to it (see "TypeFwdEq").
      if (param.Sym != nullptr and
        arg.Sym->LinkedScope->NonGenericScope == (
          param.Sym->LinkedScope != nullptr ? param.Sym->LinkedScope->NonGenericScope : nullptr)) {
        return false;
      }

      // Check each super type on the type for a FwdRef or FwdMut
      // superimposition extension and select it.
      // Todo: Move to "marker_sups" util package?
      auto const &fwd_target = both_ref ? *FWD_REF : *FWD_MUT;
      auto candidates = Vec<TypeSymbol const*>{arg.Sym};
      candidates.AppendRange(type_members::SuperClassTypes(*arg.Sym));
      for (const auto candidate : candidates) {
        if (not IsTemplate(*candidate, fwd_target, arg_scope)) { continue; }
        const auto target = SymArgGroup(*candidate).At("T");
        if (target == nullptr or target->TypeVal == nullptr) { continue; }
        auto inner = TypeRef::Of(*target->TypeVal, param_scope);
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
      for (const auto sup_sym : type_members::SuperClassTypes(*mock.Sym)) {
        const auto sup = TypeRef::OfResolved(*sup_sym, mock_scope);
        if (type_predicates::IsTypeFunc(sup, mock_scope) and AssignableCore(sup, func, mock_scope, func_scope, true)) {
          return true;
        }
      }
      return function_values::MatchFunctionValue(mock, func, func_scope).has_value();
    }

    /// [CHECKED]
    /// If the type references are "Self", then get the true
    /// class types (depending on the scopes that they are read);
    /// otherwise use the original symbols. If one isn't "Self",
    /// then return a pair of nullptrs; no special "Self"
    /// comparison needed. Otherwise
    auto ResolveSelfPair(
      TypeRef const &lhs, TypeRef const &rhs, Scope const &lhs_scope, Scope const &rhs_scope, bool &both_self)
      -> Pair<TypeSymbol*, TypeSymbol*> {
      //
      using generate::common_types_precompiled::SELF_TYPE;
      const auto lhs_self = lhs.Sym->IsSelf();
      const auto rhs_self = rhs.Sym->IsSelf();
      both_self = lhs_self and rhs_self;

      //
      auto lhs_sym = lhs_self ? rhs_scope.GetTypeSymbol(SELF_TYPE.get(), false) : lhs.Sym;
      auto rhs_sym = rhs_self ? lhs_scope.GetTypeSymbol(SELF_TYPE.get(), false) : rhs.Sym;

      //
      if (lhs_sym == nullptr or rhs_sym == nullptr) { return {nullptr, nullptr}; }
      if (lhs_self and rhs_sym->Type != nullptr) { lhs_sym = lhs_sym->AsClassSymbol(); }
      if (rhs_self and lhs_sym->Type != nullptr) { rhs_sym = rhs_sym->AsClassSymbol(); }
      return {lhs_sym, rhs_sym};
    }

    /// [CHECKED]
    /// Whether or not two resolved symbols are the same type
    /// (under a convention), by getting the type id and comparing
    /// them.
    auto SameId(
      TypeSymbol const &lhs_sym, TypeSymbol const &rhs_sym, const ConventionTag conv,
      Scope const &lhs_scope, Scope const &rhs_scope) -> bool {
      // Pointer equality shortcut. Then, get the ids of the
      // two types based on the scopes provided, and check
      // that they match.
      if (&lhs_sym == &rhs_sym) { return true; }
      auto const lhs_id = lhs_scope.TypeIdOfSym(lhs_sym, static_cast<std::uint64_t>(conv));
      return lhs_id != nullptr and lhs_id == rhs_scope.TypeIdOfSym(rhs_sym, static_cast<std::uint64_t>(conv));
    }

    /// [FWD]
    auto AssignableArgs(
      TypeSymbol const &target_sym, TypeSymbol const &value_sym, Scope const &target_scope,
      Scope const &value_scope) -> bool;

    /// [CHECKED]
    /// Check to type identity, shortcutting on never types,
    /// then a nullptr and convention check. Finally we do a
    /// "Self"-pair resolution/comparison, before checking
    /// the identity flag on the 2 symbols.
    auto SameType(
      TypeRef const &lhs, TypeRef const &rhs, Scope const &lhs_scope, Scope const &rhs_scope) -> bool {
      // Never and convention shortcut guards to avoid unnecessary
      // symbol lookups.
      if (lhs.IsNever or rhs.IsNever) { return lhs.IsNever and rhs.IsNever; }
      if (lhs.Sym == nullptr or rhs.Sym == nullptr or lhs.Conv != rhs.Conv) { return false; }

      // Do the "Self" resolution for the two types, and if they
      // are both "Self" types, return true.
      auto both_self = false;
      auto [lhs_sym, rhs_sym] = ResolveSelfPair(lhs, rhs, lhs_scope, rhs_scope, both_self);
      if (both_self) { return true; }

      // The lhs_sym and rhs_sym are the class-resolved versions
      // of the self symbols (at this point either 0 or 1 is Self;
      // If neither resolved, do a simple "id ==" under scope.
      if (lhs_sym == lhs.Sym and rhs_sym == rhs.Sym) { return lhs.Id != nullptr and lhs.Id == rhs.Id; }
      return lhs_sym != nullptr and SameId(*lhs_sym, *rhs_sym, lhs.Conv, lhs_scope, rhs_scope);
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
      if (target.Sym == nullptr or value.Sym == nullptr) { return false; }

      // Run the "Self" check; if both types are "Self" then
      // return true.
      auto both_self = false;
      auto [target_sym, value_sym] = ResolveSelfPair(target, value, target_scope, value_scope, both_self);
      if (both_self) { return true; }
      if (target_sym == nullptr) { return false; }

      // If we are allowing variant checks, then check the value
      // can be accepted by the target variant; return early.
      if (widen_variant and VariantAccepts(target, value, target_scope, value_scope)) {
        return true;
      }

      // Check the conventions are compatible. A mismatch here,
      // aside from permitted coalescing, is an early return false.
      if (not ConventionTagEq(target.Conv, value.Conv)) { return false; }

      // Next do the standard id comparison check for the target
      // and value symbols from the "Self" check.
      const auto same = target_sym == target.Sym and value_sym == value.Sym
        ? target.Id != nullptr and target.Id == value.Id
        : SameId(*target_sym, *value_sym, ConventionTag::MOV, target_scope, value_scope);
      if (same) { return true; }

      // Different templates: a function mock against a function
      // type, or a forwarding type against its target.
      if (target_sym->Type != value_sym->Type) {
        // The target is a $MockType, so check it against FunXXX
        // family overload types.
        if (target_sym->IsMock()) {
          return MockMatches(
            TypeRef::OfResolved(*target_sym, target_scope, target.Conv), value, target_scope, value_scope);
        }

        // Same as above but the other way around. Todo: Are both
        // ways needed?
        if (value_sym->IsMock()) {
          return MockMatches(
            TypeRef::OfResolved(*value_sym, value_scope, value.Conv), target, value_scope, target_scope);
        }

        // Otherwise, consider a forwarding check, of "Vec" to the
        // "&View" type for example.
        return ForwardsTo(
          TypeRef::OfResolved(*value_sym, value_scope, value.Conv),
          TypeRef::OfResolved(*target_sym, target_scope, target.Conv),
          value_scope, target_scope);
      }

      // If we reach this point, then the templates are the same,
      // but the ids are not, and therefore we need to consider
      // the generic arguments.
      return AssignableArgs(*target_sym, *value_sym, target_scope, value_scope);
    }

    /// [CHECKED]
    /// Run the assignable core for pairs of generic arguments,
    /// but also consider the variadic case.
    auto AssignableArgs(
      TypeSymbol const &target_sym, TypeSymbol const &value_sym, Scope const &target_scope,
      Scope const &value_scope) -> bool {
      // Get references to the generic arguments of both the
      // types, and determine their "arity".
      auto const &target_args = SymArgGroup(target_sym).Args;
      auto const &value_args = SymArgGroup(value_sym).Args;
      const auto arity = MatchArgListArity(
        target_sym.Type, target_args, value_args, value_scope);

      // If the lists are incompatible (by size), then shortcut
      // return false.
      if (not arity.Compatible) { return false; }

      // The length is the same, so iterate to it and perform
      // the equality logic.
      for (auto i = 0uz; i < arity.FixedLen; ++i) {
        auto const &t = *target_args[i];
        auto const &v = *value_args[i];
        // const auto ok = t.TypeVal != nullptr and v.TypeVal != nullptr
        //   ? Assignable(*t.TypeVal, *v.TypeVal, target_scope, value_scope)
        //   : t.CompVal != nullptr and v.CompVal != nullptr
        //   ? TypeEq(*t.CompVal, *v.CompVal, target_scope, value_scope)
        //   : false;
        const auto ok = t.TypeVal != nullptr and v.TypeVal != nullptr
          ? (t.TypeVal->IsSelfType() and v.TypeVal->IsSelfType()) or AssignableCore(
            TypeRef::Of(*t.TypeVal, target_scope), TypeRef::Of(*v.TypeVal, value_scope), target_scope, value_scope,
            true)
          : t.CompVal != nullptr and v.CompVal != nullptr and TypeEq(*t.CompVal, *v.CompVal, target_scope, value_scope);

        // The first mismatch is a false, as all generic arguments
        // need to match.
        if (not ok) { return false; }
      }

      // No early breaks => no mismatches, so all generics have
      // matched => containing types also match.
      return true;
    }

    /// [CHECKED]
    /// Helper to check if two types are either the same type by
    /// pointer (shortcut) or they are both "Self": auto match.
    auto BothSelfOrSame(
      TypeAst const &lhs_type, TypeAst const &rhs_type) -> bool {
      // Check by pointer then by "Self".
      return (&lhs_type == &rhs_type) or (lhs_type.IsSelfType() and rhs_type.IsSelfType());
    }

    /// What a relaxed match binds: a generic's name, and the type
    /// or comp value opposite it. "Written" is the type as the
    /// matched side spelled it, kept for the one case a resolved
    /// type cannot cover: an open instantiation nothing has made
    /// yet ("Mutex[T=T]" as the class writes it) resolves to no
    /// symbol, and is still what the generic is bound to. Todo:
    /// Remove this "TypeWritten" field.
    struct RelaxedBinding {
      Shared<TypeIdentifierAst> Name;
      TypeRef Type;
      Shared<ExpressionAst> Comp = nullptr;
      Shared<TypeAst> TypeWritten = nullptr;
    };

    using RelaxedBindings = Vec<RelaxedBinding>;

    /// [CHECKED]
    /// Given a vector of bindings, find the binding for a given
    /// name. Matches on the "Name" field of a binding.
    auto FindRelaxedBinding(
      RelaxedBindings const &bindings, TypeIdentifierAst const &name) -> RelaxedBinding const* {
      // Iterate each binding and compare on the "Name" field;
      // return the first match.
      for (auto const &b : bindings) { if (*b.Name == name) { return &b; } }
      return nullptr;
    }

    /// What every step of one relaxed match reads: the two sides' scopes, the bindings made so far, and what is checked.
    struct RelaxedCtx {
      Scope const &LhsScope;
      Scope const &RhsScope;
      RelaxedBindings &Bindings;
      bool CheckVariant;
      bool CheckConstraints;
    };

    auto RelaxedMatch(
      TypeRef const &lhs, Shared<TypeAst> const &lhs_type, TypeAst const &rhs_type, RelaxedCtx &ctx) -> bool;

    /// A comp value's identity, which compares it by meaning ("1_uz + 1_uz" is "2_uz").
    auto CompIdentityOf(ExpressionAst const &comp, Scope const &scope) -> Str {
      auto out = Str();
      comp_generics::CompExprIdentity(comp, scope, out);
      return out;
    }

    /// A type generic of the pattern binds to the type opposite it - the same type each time it is named
    /// ("Pair[T, T]") - which has to satisfy its constraints.
    auto RelaxedBindType(
      Shared<TypeIdentifierAst> name, TypeSymbol const &generic, TypeRef const &lhs, Shared<TypeAst> const &lhs_type,
      RelaxedCtx &ctx) -> bool {
      if (const auto existing = FindRelaxedBinding(ctx.Bindings, *name); existing == nullptr) {
        ctx.Bindings.EmplaceBack(RelaxedBinding{
          .Name = std::move(name), .Type = lhs, .Comp = nullptr, .TypeWritten = lhs_type
        });
      }
      else if (existing->Type.Sym != nullptr and lhs.Sym != nullptr
        and not SameType(existing->Type, lhs, ctx.LhsScope, ctx.LhsScope)) {
        return false;
      }
      // A type that resolved to no symbol has nothing to check.
      return not ctx.CheckConstraints or lhs.Sym == nullptr
        or ConstraintsHold(generic.GenericConstraints, lhs, ctx.RhsScope, ctx.LhsScope);
    }

    /// "RelaxedBindType" for a comp generic: it binds to the value opposite it, the same value each time it is named
    /// ("P2[n, n]"). Its type is checked where the value is bound to it, as a lone comp argument's is.
    auto RelaxedBindComp(
      Shared<TypeIdentifierAst> name, Shared<ExpressionAst> value, RelaxedCtx &ctx) -> bool {
      if (auto const *const existing = FindRelaxedBinding(ctx.Bindings, *name); existing != nullptr) {
        return existing->Comp == nullptr or value == nullptr
          or CompIdentityOf(*existing->Comp, ctx.LhsScope) == CompIdentityOf(*value, ctx.LhsScope);
      }
      ctx.Bindings.EmplaceBack(RelaxedBinding{
        .Name = std::move(name), .Type = {}, .Comp = std::move(value), .TypeWritten = nullptr
      });
      return true;
    }

    /// A type argument opposite a type argument: matched as a type.
    auto RelaxedMatchTypeArg(
      GenericArgumentAst const &l, GenericArgumentAst const &r, RelaxedCtx &ctx) -> bool {
      return l.TypeVal != nullptr and RelaxedMatch(TypeRef::Of(*l.TypeVal, ctx.LhsScope), l.TypeVal, *r.TypeVal, ctx);
    }

    auto RelaxedMatchCompValue(Shared<ExpressionAst> const &l, ExpressionAst const &r, RelaxedCtx &ctx) -> bool;

    /// "RelaxedMatch" for two comp packs ("(0_uz, ns)" against "(0_uz, 1_uz, 2_uz)"), as a type pack's tuple is matched:
    /// element by element, a trailing pack of the pattern binding what is left as a pack of its own.
    auto RelaxedMatchCompPack(
      Shared<ExpressionAst> const &l, TupleLiteralAst const &l_pack, TupleLiteralAst const &r_pack,
      RelaxedCtx &ctx) -> bool {
      auto const &r_elems = r_pack.Elems;
      const auto tail = not r_elems.IsEmpty() and packs::IsUnboundCompPackNamed(*r_elems.Back(), ctx.RhsScope);
      const auto fixed_len = tail ? r_elems.Len() - 1 : r_elems.Len();
      if (tail ? l_pack.Elems.Len() < fixed_len : l_pack.Elems.Len() != fixed_len) { return false; }

      // Each element is held through the pack it is in, which owns it.
      for (auto i = 0uz; i < fixed_len; ++i) {
        if (not RelaxedMatchCompValue(Shared<ExpressionAst>(l, l_pack.Elems[i].get()), *r_elems[i], ctx)) { return false; }
      }
      if (not tail) { return true; }
      auto rest = Vec<Unique<ExpressionAst>>();
      for (auto i = fixed_len; i < l_pack.Elems.Len(); ++i) { rest.EmplaceBack(AstClone(l_pack.Elems[i])); }
      return RelaxedBindComp(
        TypeIdentifierAst::FromIdentifier(*r_elems.Back()->ToUnchecked<IdentifierAst>()),
        MakeShared<TupleLiteralAst>(nullptr, std::move(rest), nullptr), ctx);
    }

    /// A comp value opposite a comp value: a pack element by element, a comp generic bound to it, anything else the
    /// same value by identity.
    auto RelaxedMatchCompValue(
      Shared<ExpressionAst> const &l, ExpressionAst const &r, RelaxedCtx &ctx) -> bool {
      auto const *const l_pack = l != nullptr ? l->To<TupleLiteralAst>() : nullptr;
      if (auto const *const r_pack = r.To<TupleLiteralAst>(); r_pack != nullptr and l_pack != nullptr) {
        return RelaxedMatchCompPack(l, *l_pack, *r_pack, ctx);
      }
      auto const *const r_id = r.To<IdentifierAst>();
      auto const *const r_var = r_id != nullptr ? ctx.RhsScope.GetVarSymbol(r_id) : nullptr;
      if (r_id != nullptr and (r_var == nullptr or r_var->IsCompGeneric())) {
        return RelaxedBindComp(TypeIdentifierAst::FromIdentifier(*r_id), l, ctx);
      }
      return l != nullptr and CompIdentityOf(*l, ctx.LhsScope) == CompIdentityOf(r, ctx.RhsScope);
    }

    /// A comp argument opposite a comp argument: its value, held through the type it is written in ("lhs_type").
    auto RelaxedMatchCompArg(
      GenericArgumentAst const &l, GenericArgumentAst const &r, Shared<TypeAst> const &lhs_type, RelaxedCtx &ctx) -> bool {
      const auto held = l.CompVal != nullptr ? Shared<ExpressionAst>(lhs_type, l.CompVal.get()) : nullptr;
      return RelaxedMatchCompValue(held, *r.CompVal, ctx);
    }

    /// A trailing pack in the pattern ("Tup[First, ..Rest]", "A[m, rest]") binds the arguments past the fixed ones, as
    /// the tuple a variadic generic is bound to in an instance. An instance records its own pack as that tuple already,
    /// as one named argument ("P[Ts=Tup[S32, Bool]]", "A[ns=(1_uz, 2_uz)]"), and a lone argument naming a pack is one
    /// too; only a positional list is gathered into one. An argument of the other kind is no part of the pack.
    auto RelaxedGatherPack(
      GenericArgumentAst const &pack_arg, Vec<Unique<GenericArgumentAst>> const &lhs_args, const std::size_t fixed_len,
      Shared<TypeAst> const &lhs_type, TypeAst const &rhs_type, RelaxedCtx &ctx) -> bool {
      const auto recorded = lhs_args.Len() == fixed_len + 1 and lhs_args[fixed_len]->Name != nullptr;
      const auto lone = lhs_args.Len() == fixed_len + 1;

      if (pack_arg.CompVal != nullptr) {
        auto rest = Vec<Unique<ExpressionAst>>();
        for (auto i = fixed_len; i < lhs_args.Len(); ++i) {
          if (lhs_args[i]->CompVal == nullptr) { return false; }
          rest.EmplaceBack(AstClone(lhs_args[i]->CompVal));
        }
        auto pack = lone and (recorded or packs::NamesPack(*lhs_args[fixed_len], ctx.LhsScope))
          ? Shared<ExpressionAst>(lhs_type, lhs_args[fixed_len]->CompVal.get())
          : MakeShared<TupleLiteralAst>(nullptr, std::move(rest), nullptr);
        return RelaxedBindComp(
          TypeIdentifierAst::FromIdentifier(*pack_arg.CompVal->ToUnchecked<IdentifierAst>()), std::move(pack), ctx);
      }

      auto pack_name = static_shared_cast<TypeIdentifierAst>(
        mut_shared_cast(pack_arg.TypeVal->WithoutGenerics()->WithoutConvention()));
      auto rest = Vec<Shared<TypeAst>>();
      for (auto i = fixed_len; i < lhs_args.Len(); ++i) {
        if (lhs_args[i]->TypeVal == nullptr) { return false; }
        rest.EmplaceBack(lhs_args[i]->TypeVal);
      }
      auto pack = lone and (recorded or packs::NamesPack(*lhs_args[fixed_len], ctx.LhsScope))
        ? rest[0]
        : generate::common_types::TupleType(rhs_type.PosStart(), std::move(rest));
      if (FindRelaxedBinding(ctx.Bindings, *pack_name) == nullptr) {
        ctx.Bindings.EmplaceBack(RelaxedBinding{
          .Name = std::move(pack_name), .Type = {}, .Comp = nullptr, .TypeWritten = std::move(pack)
        });
      }
      return true;
    }

    /// The master relaxed matcher. This does type comparison and loads up the bindings struct with information gained
    /// whilst comparing. Particularly key is the generics that are obtained. "Vec[T]" vs "Vec[Str]" obtains "T=Str".
    auto RelaxedMatch(
      TypeRef const &lhs, Shared<TypeAst> const &lhs_type, TypeAst const &rhs_type, RelaxedCtx &ctx) -> bool {
      auto const &lhs_scope = ctx.LhsScope;
      auto const &rhs_scope = ctx.RhsScope;

      // Strip the rhs type down, and get the symbol for it. A generic rhs binds the lhs type.
      const auto stripped_rhs = mut_shared_cast(rhs_type.WithoutGenerics()->WithoutConvention());
      const auto rhs_head = rhs_scope.GetTypeSymbol(stripped_rhs.get());
      if (rhs_head == nullptr) { return false; }
      if (rhs_head->IsTypeGeneric()) {
        return RelaxedBindType(static_shared_cast<TypeIdentifierAst>(stripped_rhs), *rhs_head, lhs, lhs_type, ctx);
      }

      if (not ConventionTagEq(lhs.Conv, ConventionTagOf(rhs_type))) { return false; }

      // An open instantiation nothing has made yet resolves to no symbol, and is read as the matched side spelled it.
      const auto stripped_lhs = mut_shared_cast(lhs_type->WithoutGenerics()->WithoutConvention());
      auto const *const lhs_head = lhs.Sym != nullptr
        ? lhs.Sym
        : lhs_type != nullptr
        ? lhs_scope.GetTypeSymbol(stripped_lhs.get())
        : nullptr;
      if (lhs_head == nullptr) { return false; }

      // A variant pattern takes any of its members.
      if (ctx.CheckVariant and type_predicates::IsTypeVariant(*rhs_head, rhs_scope)) {
        if (auto const *const rhs_sym = rhs_scope.GetTypeSymbol(&rhs_type); rhs_sym != nullptr) {
          for (auto const &member : VariantMember(*rhs_sym->FqName(), rhs_scope)) {
            if (RelaxedMatch(lhs, lhs_type, *member, ctx)) { return true; }
          }
        }
      }

      if (lhs_head->Type != rhs_head->Type) { return false; }

      auto const &rhs_args = InstanceArgs(rhs_type, rhs_scope);

      // A template written over its own parameters ("BufWrite[W=W]") resolves to the template itself, which records no
      // arguments; it stands for its parameters, which the pattern's constraints still apply to. Only where the pattern
      // asks for arguments: a bare "Tup" matches a bare "Tup", whose parameters are not being matched at all.
      auto lhs_self = Shared<TypeAst>();
      if (not rhs_args.IsEmpty() and lhs.Sym != nullptr and lhs.Sym->Kind == TypeKind::Class
        and lhs.Sym->InstanceOf == nullptr and lhs.Sym->Alias == nullptr and lhs.Sym->Type != nullptr
        and not lhs.Sym->Type->GnParamGroup->Params.IsEmpty()
        and lhs.Sym->Name->GnArgGroup->Args.IsEmpty()) { lhs_self = lhs.Sym->GenericSelfName(); }
      auto const &args_owner = lhs_type;
      auto const &lhs_args = lhs_self != nullptr
        ? lhs_self->LastTypePart()->GnArgGroup->Args
        : lhs.Sym == nullptr
        ? InstanceArgs(*lhs_type, lhs_scope)
        : SymArgGroup(*lhs.Sym).Args;
      const auto arity = MatchArgListArity(lhs_head->Type, lhs_args, rhs_args, rhs_scope);
      if (not arity.Compatible) { return false; }
      if (ctx.CheckConstraints and arity.Pack != nullptr
        and not PackConstraintsSatisfied(*arity.Pack, lhs_args, arity.FixedLen, rhs_scope, lhs_scope)) { return false; }

      // The arguments one against one, each of the kind the pattern's is.
      for (auto i = 0uz; i < arity.FixedLen; ++i) {
        auto const &l = *lhs_args[i];
        auto const &r = *rhs_args[i];
        const auto matched = r.TypeVal != nullptr
          ? RelaxedMatchTypeArg(l, r, ctx)
          : RelaxedMatchCompArg(l, r, args_owner, ctx);
        if (not matched) { return false; }
      }
      return arity.Pack == nullptr or RelaxedGatherPack(*arity.Pack, lhs_args, arity.FixedLen, args_owner, rhs_type, ctx);
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::ConventionEq(
  TypeAst const &lhs_type,
  TypeAst const &rhs_type)
  -> bool {
  // The convention equality checker runs a tag equality on
  // the extracted tags. Nullptr-safe.
  return ConventionTagEq(ConventionTagOf(lhs_type), ConventionTagOf(rhs_type));
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::TypeEq(
  TypeAst const &lhs_type, TypeAst const &rhs_type,
  Scope const &lhs_scope, Scope const &rhs_scope) -> bool {
  // Check if they're "Self" or strictly equal, otherwise run
  // through the core id comparison test.
  if (BothSelfOrSame(lhs_type, rhs_type)) { return true; }
  auto const lhs_id = lhs_scope.TypeIdOf(lhs_type);
  return lhs_id != nullptr and lhs_id == rhs_scope.TypeIdOf(rhs_type);
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
  TypeAst const &target_type, TypeAst const &value_type,
  Scope const &target_scope, Scope const &value_scope) -> bool {
  // Check if they're "Self" or strictly equal, otherwise run
  // through the core assignment checker.
  return BothSelfOrSame(target_type, value_type) or AssignableCore(
    TypeRef::Of(target_type, target_scope), TypeRef::Of(value_type, value_scope), target_scope, value_scope, true);
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::Assignable(
  TypeRef const &target, TypeRef const &value,
  Scope const &target_scope, Scope const &value_scope) -> bool {
  // Run through the core assignment checker.
  return AssignableCore(target, value, target_scope, value_scope, true);
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::TypeEq(
  ExpressionAst const &lhs_expr, ExpressionAst const &rhs_expr, Scope const &lhs_scope,
  Scope const &rhs_scope) -> bool {
  // Generate the identity keys for the comp argument's values,
  // and compare them for equality (allows 1 + 1 to match 2).
  auto lhs_identity = Str();
  auto rhs_identity = Str();
  comp_generics::CompExprIdentity(lhs_expr, lhs_scope, lhs_identity);
  comp_generics::CompExprIdentity(rhs_expr, rhs_scope, rhs_identity);
  return lhs_identity == rhs_identity;
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::TypeFwdEq(
  TypeAst const &arg_type, TypeAst const &param_type, Scope const &arg_scope, Scope const &param_scope) -> bool {
  // Wrap the internal forwarding call checker.
  return ForwardsTo(
    TypeRef::Of(arg_type, arg_scope), TypeRef::Of(param_type, param_scope), arg_scope, param_scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::RelaxedTypeEq(
  TypeAst const &lhs_type, TypeAst const &rhs_type, Scope const &lhs_scope, Scope const &rhs_scope,
  GenericInferenceMap &generic_args, const bool check_variant, const bool check_constraints) -> bool {
  // The matched type is held for as long as what is bound from
  // it: owned when it is shared, else borrowed as before. Just
  // accept this weird "Shared" construction, it works.
  auto held = std::const_pointer_cast<TypeAst>(
    std::static_pointer_cast<TypeAst const>(lhs_type.weak_from_this().lock()));
  if (held == nullptr) { held = Shared<TypeAst>(Shared<void>(), const_cast<TypeAst*>(&lhs_type)); }

  // Create the empty bindings, and call the internal relaxed
  // equality method to fill the bindings whilst comparing the
  // 2 types.
  auto bindings = RelaxedBindings();
  auto ctx = RelaxedCtx{
    .LhsScope = lhs_scope, .RhsScope = rhs_scope, .Bindings = bindings, .CheckVariant = check_variant,
    .CheckConstraints = check_constraints
  };
  const auto matched = RelaxedMatch(TypeRef::Of(lhs_type, lhs_scope), held, rhs_type, ctx);

  // Write the bindings' values into the generic args, pulling either the comp or type values into the generic
  // inference. Each name is bound once ("RelaxedBind*"), and an argument already in the map is kept, for both kinds.
  for (auto &b : bindings) {
    if (b.Comp != nullptr) { generic_args.insert({b.Name, std::move(b.Comp)}); }
    else if (b.TypeWritten != nullptr) { generic_args.insert({b.Name, std::move(b.TypeWritten)}); }
  }

  return matched;
}

auto spp::analyse::utils::type_compare::UnmetConstraint(
  Vec<Shared<TypeAst>> const &constraints,
  TypeSymbol const &concrete_sym,
  const bool is_self,
  Scope const &constraints_owner_scope,
  Scope const &concrete_scope)
  -> TypeAst const* {
  using generate::common_types_precompiled::THREAD_SAFE;
  const auto type_sym = &concrete_sym;

  // The concrete type and each class it is superimposed as,
  // resolved where each is read.
  const auto concrete = const_cast<TypeSymbol*>(type_sym);
  auto sup_info = Vec<Pair<TypeRef, Scope const*>>{};
  if (is_self and not type_sym->IsTypeGeneric() and type_sym->LinkedScope != nullptr) {
    sup_info.EmplaceBack(TypeRef::OfResolved(*concrete, *type_sym->LinkedScope), type_sym->LinkedScope);
  }

  // Get all the sup scopes of the concrete type, which we
  // compare against required constraints. For generic type
  // symbols, that generic's constraints are stored on the
  // symbol. Todo: Condense this block.
  const auto sup_scopes = type_sym->LinkedScope
    ? type_sym->LinkedScope->SupScopes()
    : type_sym->GenericConstraints | genex::views::transform([&](auto const &constraint) {
      return constraints_owner_scope.GetTypeSymbol(constraint.get())->LinkedScope;
    }) | genex::to<Vec>();
  sup_info.EmplaceBack(TypeRef::OfResolved(*concrete, concrete_scope), &concrete_scope);
  for (auto const *sup_scope : sup_scopes) {
    if (sup_scope->TySym == nullptr or AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    sup_info.EmplaceBack(TypeRef::OfResolved(*sup_scope->TySym, *sup_scope), sup_scope);
  }

  // Compare each constraint against the concrete type and
  // its supertypes.
  for (auto const &constraint : constraints) {
    // Prevent a non-thread-safe type from being used when
    // thread safety is required. Instead of auto injecting
    // as this can blow up generic instantiations.
    if (constraint->LastTypePart()->Name == THREAD_SAFE->LastTypePart()->Name
      and TypeEq(*constraint, *THREAD_SAFE, constraints_owner_scope, constraints_owner_scope)) {
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

/// [CHECKED]
auto spp::analyse::utils::type_compare::VariantMember(
  TypeAst const &type, Scope const &scope) -> Vec<Shared<TypeAst>> {
  // A type based overload of the variant member detection,
  // which flattens and deduplicates.
  const auto variants = type.LastTypePart()->GnArgGroup->At("Variants");
  if (variants != nullptr and variants->TypeVal != nullptr) {
    return FlattenVariants(*variants->TypeVal, scope)
      | genex::views::transform([](auto const &x) { return x.first; })
      | genex::to<Vec>();
  }

  return VariantMembers(TypeRef::Of(type, scope), scope)
    | genex::views::transform([](auto const &x) { return AsType(x); })
    | genex::to<Vec>();
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::VariantMembers(
  TypeRef const &ref, Scope const &scope) -> Vec<TypeRef> {
  // Never types won't have variant members so guard against
  // that at the start.
  const auto sym = ref.IsNever ? nullptr : ref.Sym;
  if (sym == nullptr) { return {}; }

  // Its identity lists its members, flattened and in order, each what it means; its name spells them as written where
  // it was made, which they need not mean here.
  if (ref.Id != nullptr and scopes::HeadOf(ref.Id).Kind == scopes::InstanceKey::Tag::Variant) {
    auto members = Vec<TypeRef>();
    for (const auto member : scopes::HeadOf(ref.Id).Members) {
      if (const auto type = scope.TypeAstOf(member); type != nullptr) { members.EmplaceBack(TypeRef::Of(*type, scope)); }
    }
    return members;
  }

  // Get the generic arguments under the variadic generic
  // parameter "Variants".
  const auto variants = SymArgGroup(*sym).At("Variants");
  if (variants == nullptr or variants->TypeVal == nullptr) { return {}; }

  // Run the variant list through the variant flattener, removing
  // duplicates and flattening variants into one root level type.
  return FlattenVariants(*variants->TypeVal, scope)
    | genex::views::transform([](auto const &x) { return x.second; })
    | genex::to<Vec>();
}

auto spp::analyse::utils::type_compare::TemplateOf(
  TypeSymbol const &sym,
  Scope const &scope)
  -> TypeSymbol* {
  // Followed until nothing changes, capped against a cycle.
  auto *s = const_cast<TypeSymbol*>(&sym);
  for (auto step = 0; step < 8; ++step) {
    auto *next = s;
    if (s->Kind == TypeKind::GenericParam and s->ParamId != 0) {
      if (auto *const bound = scope.Canon(*s); bound != nullptr) { next = bound; }
    }
    else if ((s->Kind == TypeKind::GenericArg or s->IsSelf()) and s->LinkedScope != nullptr
      and s->LinkedScope->TySym != nullptr) {
      next = s->LinkedScope->TySym.get();
    }
    else if (s->Alias != nullptr) {
      next = s->AliasTarget(scope);
    }
    if (next == s) { break; }
    s = next;
  }
  return s->InstanceOf != nullptr ? s->InstanceOf : s;
}

/// [CHECKED]
auto spp::analyse::utils::type_compare::IsTemplate(
  TypeSymbol const &sym, TypeAst const &tmpl, Scope const &scope) -> bool {
  // Get the template's symbol and check if the template of
  // the symbol is the template's template symbol (a template's
  // template symbol is itself).
  const auto tmpl_sym = scope.GetTypeSymbol(&tmpl);
  return tmpl_sym != nullptr and TemplateOf(sym, scope) == TemplateOf(*tmpl_sym, scope);
}
