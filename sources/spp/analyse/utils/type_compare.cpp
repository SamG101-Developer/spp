module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_compare;
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
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::type_compare {
  namespace {
    /** How two written generic argument lists line up, for the type they were both written for. */
    struct ArgListArity {
      /** Whether the two lengths can describe the same type at all. */
      bool Compatible;

      /** How many leading arguments are compared one against one; anything past this is the pack. */
      std::size_t FixedLen;

      /** The variadic parameter the right-hand-side's trailing argument names, if it names one. */
      TypeSymbol *Pack;
    };

    /** Whether "type" satisfies every constraint ("EnforceGenericConstraintsOneArg"); true when there are none. */
    auto ConstraintsHold(
      Vec<Shared<TypeAst>> const &constraints,
      TypeAst const &type,
      Scope const &constraint_scope,
      Scope const &type_scope)
      -> bool {
      return constraints.IsEmpty()
        or EnforceGenericConstraintsOneArg(constraints, type, constraint_scope, type_scope) == nullptr;
    }

    /** The generic arguments a symbol holds, as its instantiation records them; an alias holds its target's. */
    auto SymArgGroup(
      TypeSymbol const &sym)
      -> GenericArgumentGroupAst const& {
      if (sym.Alias != nullptr and sym.Alias->Resolved != nullptr) { return *sym.Alias->Resolved->LastTypePart()->GnArgGroup; }
      return *sym.Name->GnArgGroup;
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
      // An alias's instantiation has a scope of its own, so the target's arguments are read off what it resolves to.
      auto const *const sym = scope.GetTypeSymbol(&type);
      if (sym != nullptr and sym->Alias != nullptr and sym->Alias->Resolved != nullptr) { return SymArgGroup(*sym).Args; }

      // An alias name can also resolve straight to its target's symbol, with no alias instantiation of its own in
      // between: a type is stamped with what it resolved to where it was written, and a stamp made through an alias
      // names the target. That symbol holds the target's arguments ("Var[Variants=..]"), and the written ones are the
      // alias's ("Res[T, E]") - lining the written list up against the target's would leave them unmatched.
      if (sym != nullptr and sym->Alias == nullptr and not type.LastTypePart()->GnArgGroup->Args.IsEmpty()) {
        auto const *const head = scope.GetTypeSymbol(type.WithoutGenerics()->WithoutConvention().get());
        if (head != nullptr and head->Alias != nullptr) { return sym->Name->GnArgGroup->Args; }
      }
      return type.LastTypePart()->GnArgGroup->Args;
    }

    /**
     * Line up the written generic arguments of two types.
     * @param proto The prototype both lists were written for, whose parameters say whether it is variadic at all.
     * @param lhs_args The left-hand-side's written arguments.
     * @param rhs_args The right-hand-side's written arguments, the side a pack may be written on.
     * @param rhs_scope The scope the right-hand-side's arguments are named in.
     */
    auto MatchArgListArity(
      ClassPrototypeAst const *proto,
      Vec<Unique<GenericArgumentAst>> const &lhs_args,
      Vec<Unique<GenericArgumentAst>> const &rhs_args,
      Scope const &rhs_scope)
      -> ArgListArity {
      //
      using namespace spp::asts;

      // The trailing argument is a pack only when it names a
      // variadic parameter that is still unbound.
      auto *pack = static_cast<TypeSymbol*>(nullptr);
      if (not rhs_args.IsEmpty()) {
        if (auto const &last = rhs_args.Back(); last->TypeVal != nullptr) {
          const auto sym = rhs_scope.GetTypeSymbol(last->TypeVal->WithoutGenerics().get(), false);
          if (sym != nullptr and packs::IsTypePack(*sym) and sym->AsBoundSymbol() == sym) { pack = sym; }
        }
      }

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
      TypeSymbol const &pack,
      Vec<Unique<GenericArgumentAst>> const &lhs_args,
      const std::size_t fixed_len,
      Scope const &pack_scope,
      Scope const &arg_scope)
      -> bool {
      //
      if (pack.GenericConstraints.IsEmpty()) { return true; }

      for (auto i = fixed_len; i < lhs_args.Len(); ++i) {
        if (lhs_args[i]->TypeVal == nullptr) { continue; }
        if (not ConstraintsHold(pack.GenericConstraints, *lhs_args[i]->TypeVal, pack_scope, arg_scope)) { return false; }
      }
      return true;
    }

  }
}

namespace spp::analyse::utils::type_compare {
  namespace {
    /**
     * Whether a value held as @p rhs can be taken where @p lhs is wanted: the same convention, or "&mut" given where
     * "&" is wanted, which moves a mutable borrow to an immutable one without re-borrowing. "MOV" is no convention, and
     * only matches itself.
     */
    auto ConventionTagEq(
      const ConventionTag lhs,
      const ConventionTag rhs)
      -> bool {
      if (lhs == rhs) { return true; }
      if (lhs == ConventionTag::MOV or rhs == ConventionTag::MOV) { return false; }
      return not(lhs == ConventionTag::MUT and rhs == ConventionTag::REF);
    }

    auto ConventionTagOf(
      TypeAst const &type)
      -> ConventionTag {
      auto const *conv = type.GetConvention();
      return conv != nullptr ? conv->Tag() : ConventionTag::MOV;
    }
  }

  namespace {
    auto TypeEqCore(
      TypeRef const &lhs,
      TypeRef const &rhs,
      Scope const &lhs_scope,
      Scope const &rhs_scope,
      bool check_variant)
      -> bool;

    /** The symbol a type stands for: a binding is what it is bound to. Anything else stands for itself. */
    auto CanonicalSym(
      TypeSymbol *sym)
      -> TypeSymbol* {
      return sym != nullptr and sym->Kind == TypeKind::GenericArg ? sym->AsBoundSymbol() : sym;
    }

    /** A resolved type as a written one, for the readers that still take one: its qualified name under its convention. */
    auto AsType(
      TypeRef const &ref)
      -> Shared<TypeAst> {
      auto name = ref.Sym->FqName();
      if (ref.Conv == ConventionTag::MUT) { return name->WithConvention(MakeUnique<ConventionMutAst>(nullptr, nullptr)); }
      if (ref.Conv == ConventionTag::REF) { return name->WithConvention(MakeUnique<ConventionRefAst>(nullptr)); }
      return name;
    }

    /**
     * The members listed in a variant's "Variants" argument, flattened through nested variants and without duplicates.
     * Each is paired with how it is named: as written for a member listed directly, by its symbol for one reached
     * through a nested variant.
     */
    auto FlattenVariants(
      TypeAst const &variants,
      Scope const &scope)
      -> Vec<Pair<Shared<TypeAst>, TypeRef>> {
      auto out = Vec<Pair<Shared<TypeAst>, TypeRef>>();
      const auto add_unique = [&](Shared<TypeAst> const &member, TypeRef const &ref) {
        if (genex::any_of(out, [&](auto const &x) { return TypeEqCore(ref, x.second, scope, scope, false); })) { return; }
        out.EmplaceBack(member, ref);
      };
      for (auto const *arg : variants.LastTypePart()->GnArgGroup->GetTypeArgs()) {
        const auto ref = TypeRef::Of(*arg->TypeVal, scope);
        const auto nested = type_compare::VariantMembers(ref, scope);
        if (nested.IsEmpty()) { add_unique(arg->TypeVal, ref); }
        else { for (auto const &m : nested) { add_unique(AsType(m), m); } }
      }
      return out;
    }

    /** "TypeVariantEq" on resolved types: "type" is one of the variant's members, or a variant of only its members. */
    auto VariantAccepts(
      TypeRef const &variant,
      TypeRef const &type,
      Scope const &variant_scope,
      Scope const &type_scope)
      -> bool {
      const auto variant_members = VariantMembers(variant, variant_scope);
      if (variant_members.IsEmpty()) { return false; }
      const auto type_members = VariantMembers(type, type_scope);
      if (not type_members.IsEmpty()) {
        return genex::all_of(type_members, [&](auto const &t) {
          return genex::any_of(variant_members, [&](auto const &v) { return TypeEqCore(v, t, variant_scope, type_scope, false); });
        });
      }
      return genex::any_of(variant_members, [&](auto const &v) { return TypeEqCore(v, type, variant_scope, type_scope, true); });
    }

    /** "TypeFwdEq" on resolved types: "&Str" is taken where "&StrView" is wanted, as "Str" forwards to it. */
    auto ForwardsTo(
      TypeRef const &arg,
      TypeRef const &param,
      Scope const &arg_scope,
      Scope const &param_scope)
      -> bool {
      using generate::common_types_precompiled::FWD_REF;
      using generate::common_types_precompiled::FWD_MUT;
      const auto both_ref = arg.Conv == ConventionTag::REF and param.Conv == ConventionTag::REF;
      const auto both_mut = arg.Conv == ConventionTag::MUT and param.Conv == ConventionTag::MUT;
      if (not both_ref and not both_mut) { return false; }
      if (arg.Sym == nullptr or arg.Sym->IsTypeGeneric() or arg.Sym->LinkedScope == nullptr) { return false; }

      // An argument already naming the parameter's own class is not forwarded to it (see "TypeFwdEq").
      if (param.Sym != nullptr and arg.Sym->LinkedScope->NonGenericScope == (param.Sym->LinkedScope != nullptr
        ? param.Sym->LinkedScope->NonGenericScope
        : nullptr)) { return false; }

      // The argument's own type is a candidate too, and then each type it is superimposed as.
      auto const &fwd_target = both_ref ? *FWD_REF : *FWD_MUT;
      auto candidates = Vec<TypeSymbol const*>{arg.Sym};
      candidates.AppendRange(type_members::SuperClassTypes(*arg.Sym));
      for (auto const *candidate : candidates) {
        auto const &sup_sym = *candidate;
        if (not type_compare::IsTemplate(sup_sym, fwd_target, arg_scope)) { continue; }
        auto const *const target = SymArgGroup(sup_sym).At("T");
        if (target == nullptr or target->TypeVal == nullptr) { continue; }
        auto inner = TypeRef::Of(*target->TypeVal, param_scope);
        inner.Conv = param.Conv;
        if (TypeEqCore(inner, param, param_scope, param_scope, true)) { return true; }
      }
      return false;
    }

    /** "TypeFuncEq" on resolved types: a "$" mock matches a function type one of its overloads is superimposed as. */
    auto MockMatches(
      TypeRef const &mock,
      TypeRef const &func,
      Scope const &mock_scope,
      Scope const &func_scope)
      -> bool {
      for (auto *sup_sym : type_members::SuperClassTypes(*mock.Sym)) {
        const auto sup = TypeRef{.Sym = sup_sym};
        if (type_predicates::IsTypeFunc(sup, mock_scope) and TypeEqCore(sup, func, mock_scope, func_scope, true)) {
          return true;
        }
      }
      return function_values::MatchFunctionValue(mock, func, func_scope).has_value();
    }

    auto TypeEqCore(
      TypeRef const &lhs,
      TypeRef const &rhs,
      Scope const &lhs_scope,
      Scope const &rhs_scope,
      const bool check_variant)
      -> bool {
      using generate::common_types_precompiled::SELF_TYPE;

      // "!" fits anywhere but is only met by itself; "Self" meets "Self".
      if (rhs.IsNever) { return true; }
      if (lhs.IsNever) { return false; }
      if (lhs.Sym == nullptr or rhs.Sym == nullptr) { return false; }
      const auto lhs_self = lhs.Sym->IsSelf();
      const auto rhs_self = rhs.Sym->IsSelf();
      if (lhs_self and rhs_self) { return true; }

      // "Self" against a written type is read where the other side is: an override's "Self" is the class it is written
      // in. It is followed through to that class only when the other side names one: against a symbol with no prototype
      // either (an unbound parameter, most of all) the two match by both having none, which is what lets a method
      // written in terms of "Self" register as overriding an abstract one.
      auto *lhs_sym = lhs_self ? rhs_scope.GetTypeSymbol(SELF_TYPE.get(), false) : lhs.Sym;
      auto *rhs_sym = rhs_self ? lhs_scope.GetTypeSymbol(SELF_TYPE.get(), false) : rhs.Sym;
      if (lhs_sym == nullptr or rhs_sym == nullptr) { return false; }
      if (lhs_self and rhs_sym->Type != nullptr) { lhs_sym = lhs_sym->AsClassSymbol(); }
      if (rhs_self and lhs_sym->Type != nullptr) { rhs_sym = rhs_sym->AsClassSymbol(); }

      if (check_variant and VariantAccepts(lhs, rhs, lhs_scope, rhs_scope)) { return true; }
      if (not ConventionTagEq(lhs.Conv, rhs.Conv)) { return false; }
      if (lhs_sym == rhs_sym) { return true; }
      lhs_sym = CanonicalSym(lhs_sym);
      rhs_sym = CanonicalSym(rhs_sym);
      if (lhs_sym == rhs_sym) { return true; }

      // Two instantiations of one template under one identity are one type, however many symbols stand for it: the key
      // is what identity means ("Scope::InstanceIdentityKey"), and it keys a "Self" argument by its spelling.
      if (lhs_sym->InstanceOf != nullptr and lhs_sym->InstanceOf == rhs_sym->InstanceOf
        and lhs_sym->IdentityKey == rhs_sym->IdentityKey) { return true; }

      // Different templates: a function mock against a function type, or a forwarding type against its target.
      if (lhs_sym->Type != rhs_sym->Type) {
        if (lhs_sym->IsMock()) { return MockMatches({.Sym = lhs_sym, .Conv = lhs.Conv}, rhs, lhs_scope, rhs_scope); }
        if (rhs_sym->IsMock()) { return MockMatches({.Sym = rhs_sym, .Conv = rhs.Conv}, lhs, rhs_scope, lhs_scope); }
        return ForwardsTo(
          {.Sym = rhs_sym, .Conv = rhs.Conv}, {.Sym = lhs_sym, .Conv = lhs.Conv}, rhs_scope, lhs_scope);
      }

      // One template: the arguments each instantiation records, one for one.
      auto const &lhs_args = SymArgGroup(*lhs_sym).Args;
      auto const &rhs_args = SymArgGroup(*rhs_sym).Args;
      const auto arity = MatchArgListArity(lhs_sym->Type, lhs_args, rhs_args, rhs_scope);
      if (not arity.Compatible) { return false; }
      for (auto i = 0uz; i < arity.FixedLen; ++i) {
        auto const &l = *lhs_args[i];
        auto const &r = *rhs_args[i];
        // A "Self" argument is keyed by its spelling, as an instantiation's identity keys it.
        const auto eq = l.TypeVal != nullptr and r.TypeVal != nullptr
          ? (l.TypeVal->IsSelfType() and r.TypeVal->IsSelfType()) or TypeEqCore(
            TypeRef::Of(*l.TypeVal, lhs_scope), TypeRef::Of(*r.TypeVal, rhs_scope),
            lhs_scope, rhs_scope, true)
          : l.CompVal != nullptr and r.CompVal != nullptr
          and TypeEq(*l.CompVal, *r.CompVal, lhs_scope, rhs_scope);
        if (not eq) { return false; }
      }
      return true;
    }

    /** "TypeEq" on written types, through the ref core. "Self" is keyed by spelling: it names the enclosing type, which
     * each side reads in its own scope. */
    auto TypeEqWritten(
      TypeAst const &lhs_type,
      TypeAst const &rhs_type,
      Scope const &lhs_scope,
      Scope const &rhs_scope,
      const bool check_variant)
      -> bool {
      if (&lhs_type == &rhs_type) { return true; }
      if (lhs_type.IsSelfType() and rhs_type.IsSelfType()) { return true; }
      return TypeEqCore(
        TypeRef::Of(lhs_type, lhs_scope), TypeRef::Of(rhs_type, rhs_scope), lhs_scope, rhs_scope,
        check_variant);
    }

    /**
     * What a relaxed match binds: a generic's name, and the type - or comp value - opposite it. "Written" is the type as
     * the matched side spelled it, kept for the one case a resolved type cannot cover: an open instantiation nothing has
     * made yet ("Mutex[T=T]" as the class writes it) resolves to no symbol, and is still what the generic is bound to.
     */
    struct RelaxedBinding {
      Shared<TypeIdentifierAst> Name;
      TypeRef Type;
      ExpressionAst const *Comp = nullptr;
      TypeAst const *Written = nullptr;
    };

    using RelaxedBindings = Vec<RelaxedBinding>;

    auto FindBinding(
      RelaxedBindings const &bindings,
      TypeIdentifierAst const &name)
      -> RelaxedBinding const* {
      for (auto const &b : bindings) { if (*b.Name == name) { return &b; } }
      return nullptr;
    }

    /**
     * "RelaxedTypeEq" with the matched side resolved: "rhs_type" is the pattern as written, whose generics bind to the
     * resolved types opposite them in "lhs". Only the right binds, and a generic there accepts anything, held to its own
     * constraints. The pattern is syntax, written over its block's or function's own parameters, so it is read as
     * written - its head by name, its arguments as spelled; "lhs" is read off the arguments its instantiation records.
     * The pattern's depth bounds the walk, so an instantiation whose recorded name names itself is never descended past
     * what the pattern asks of it.
     */
    auto RelaxedMatch(
      TypeRef const &lhs,
      TypeAst const *lhs_written,
      TypeAst const &rhs_type,
      Scope const &lhs_scope,
      Scope const &rhs_scope,
      RelaxedBindings &bindings,
      const bool check_variant,
      const bool check_constraints)
      -> bool {
      const auto stripped_rhs = mut_shared_cast(rhs_type.WithoutGenerics()->WithoutConvention());
      auto const *const rhs_head = rhs_scope.GetTypeSymbol(stripped_rhs.get());
      if (rhs_head == nullptr) { return false; }
      if (rhs_head->IsTypeGeneric()) {
        auto name = static_shared_cast<TypeIdentifierAst>(stripped_rhs);
        if (FindBinding(bindings, *name) == nullptr) {
          bindings.EmplaceBack(RelaxedBinding{.Name = std::move(name), .Type = lhs, .Comp = nullptr, .Written = lhs_written});
        }
        // A type that resolved to no symbol has nothing to check.
        return not check_constraints or lhs.Sym == nullptr
          or ConstraintsHold(rhs_head->GenericConstraints, *AsType(lhs), rhs_scope, lhs_scope);
      }

      if (not ConventionTagEq(lhs.Conv, ConventionTagOf(rhs_type))) { return false; }

      // An open instantiation nothing has made yet resolves to no symbol, and is read as the matched side spelled it.
      auto const *const lhs_head = lhs.Sym != nullptr
        ? lhs.Sym
        : lhs_written != nullptr
        ? lhs_scope.GetTypeSymbol(lhs_written->WithoutGenerics()->WithoutConvention().get())
        : nullptr;
      if (lhs_head == nullptr) { return false; }

      // A variant pattern takes any of its members.
      if (check_variant and type_predicates::IsTypeVariant(*rhs_head, rhs_scope)) {
        if (auto const *const rhs_sym = rhs_scope.GetTypeSymbol(&rhs_type); rhs_sym != nullptr) {
          for (auto const &member : VariantMemberTypes(*rhs_sym->FqName(), rhs_scope)) {
            if (RelaxedMatch(
              lhs, lhs_written, *member, lhs_scope, rhs_scope, bindings, true, check_constraints)) { return true; }
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
      const auto lhs_from_written = lhs.Sym == nullptr;
      auto const &lhs_args = lhs_self != nullptr
        ? lhs_self->LastTypePart()->GnArgGroup->Args
        : lhs_from_written ? InstanceArgs(*lhs_written, lhs_scope) : SymArgGroup(*lhs.Sym).Args;
      const auto arity = MatchArgListArity(lhs_head->Type, lhs_args, rhs_args, rhs_scope);
      if (not arity.Compatible) { return false; }
      if (check_constraints and arity.Pack != nullptr
        and not PackConstraintsSatisfied(*arity.Pack, lhs_args, arity.FixedLen, rhs_scope, lhs_scope)) { return false; }

      for (auto i = 0uz; i < arity.FixedLen; ++i) {
        auto const &l = *lhs_args[i];
        auto const &r = *rhs_args[i];
        if (r.TypeVal != nullptr) {
          if (l.TypeVal == nullptr or not RelaxedMatch(
            TypeRef::Of(*l.TypeVal, lhs_scope), l.TypeVal.get(), *r.TypeVal, lhs_scope, rhs_scope, bindings,
            check_variant, check_constraints)) { return false; }
          continue;
        }

        // A comp argument naming a generic binds to the value opposite it; any other has to be that value.
        if (auto const *const r_id = r.CompVal->To<IdentifierAst>(); r_id != nullptr) {
          const auto name = TypeIdentifierAst::FromIdentifier(*r_id);
          auto const *const existing = FindBinding(bindings, *name);
          if (existing != nullptr) { const_cast<RelaxedBinding*>(existing)->Comp = l.CompVal.get(); }
          else {
            bindings.EmplaceBack(
              RelaxedBinding{.Name = name, .Type = {}, .Comp = l.CompVal.get(), .Written = nullptr});
          }
          continue;
        }
        if (l.CompVal == nullptr or not(*l.CompVal == *r.CompVal)) { return false; }
      }
      return true;
    }
  }

}

auto spp::analyse::utils::type_compare::ConventionEq(
  TypeAst const &lhs_type,
  TypeAst const &rhs_type)
  -> bool {
  return ConventionTagEq(ConventionTagOf(lhs_type), ConventionTagOf(rhs_type));
}

auto spp::analyse::utils::type_compare::TypeEq(
  TypeAst const &lhs_type,
  TypeAst const &rhs_type,
  Scope const &lhs_scope,
  Scope const &rhs_scope,
  const bool check_variant)
  -> bool {
  return TypeEqWritten(lhs_type, rhs_type, lhs_scope, rhs_scope, check_variant);
}

auto spp::analyse::utils::type_compare::TypeEq(
  TypeRef const &lhs,
  TypeRef const &rhs,
  Scope const &lhs_scope,
  Scope const &rhs_scope,
  const bool check_variant)
  -> bool {
  return TypeEqCore(lhs, rhs, lhs_scope, rhs_scope, check_variant);
}

auto spp::analyse::utils::type_compare::TypeEq(
  ExpressionAst const &lhs_expr,
  ExpressionAst const &rhs_expr,
  Scope const &lhs_scope,
  Scope const &rhs_scope)
  -> bool {
  // Comp values compare by identity, each read from its own side: by value where closed ("1_uz + 1_uz" is "2_uz"),
  // by parameter where not, and through parentheses ("(n + 1_uz)" is "n + 1_uz").
  auto lhs_identity = Str();
  auto rhs_identity = Str();
  comp_generics::CompExprIdentity(lhs_expr, lhs_scope, lhs_identity);
  comp_generics::CompExprIdentity(rhs_expr, rhs_scope, rhs_identity);
  return lhs_identity == rhs_identity;
}

auto spp::analyse::utils::type_compare::TypeFwdEq(
  TypeAst const &arg_type,
  TypeAst const &param_type,
  Scope const &arg_scope,
  Scope const &param_scope)
  -> bool {
  return ForwardsTo(
    TypeRef::Of(arg_type, arg_scope), TypeRef::Of(param_type, param_scope), arg_scope, param_scope);
}

auto spp::analyse::utils::type_compare::RelaxedTypeEq(
  TypeAst const &lhs_type,
  TypeAst const &rhs_type,
  Scope const &lhs_scope,
  Scope const &rhs_scope,
  GenericInferenceMap &generic_args,
  const bool check_variant,
  const bool check_constraints) -> bool {
  // The pattern is matched against the resolved type ("RelaxedMatch"). What it binds is recorded against the
  // parameter's name as the matched side spelled it: a type argument is carried on to instantiate with, and an open
  // instantiation nothing has made yet has no resolved form to name it by. Bindings made before a match failed are
  // kept, as the caller decides what a partial match means.
  auto bindings = RelaxedBindings();
  const auto matched = RelaxedMatch(
    TypeRef::Of(lhs_type, lhs_scope), &lhs_type, rhs_type, lhs_scope, rhs_scope, bindings, check_variant,
    check_constraints);

  for (auto const &b : bindings) {
    if (b.Comp != nullptr) { generic_args[b.Name] = const_cast<ExpressionAst*>(b.Comp); }
    else if (b.Written != nullptr) { generic_args.insert({b.Name, const_cast<TypeAst*>(b.Written)}); }
  }
  return matched;
}

auto spp::analyse::utils::type_compare::EnforceGenericConstraintsOneArg(
  Vec<Shared<TypeAst>> const &constraints,
  TypeAst const &concrete_type,
  Scope const &constraints_owner_scope,
  Scope const &concrete_scope)
  -> TypeAst const* {
  // Note: concrete scope is where the type is being used; concrete_sym->LinkedScope is the scope of the type
  // definition.
  using generate::common_types_precompiled::THREAD_SAFE;

  // Determine the concrete symbol, and if non-generic, add its scope.
  const auto concrete_sym = concrete_scope.GetTypeSymbol(&concrete_type);
  if (concrete_sym == nullptr) { return nullptr; } // Failsafe for some $ClosureTypes

  auto sup_info = Vec<Pair<Shared<TypeAst>, Scope const*>>{};
  if (concrete_type.IsSelfType() and not concrete_sym->IsTypeGeneric()) {
    // Todo: might need to keep the self sym, mapped to fq
    sup_info.EmplaceBack(concrete_sym->FqName(), concrete_sym->LinkedScope);
  }

  // Get all the sup scopes of the concrete type (none for generic).
  // Using "SupScopes" not "SupTypes" because we need both the scopes and types.
  const auto sup_scopes = concrete_sym->LinkedScope
    ? concrete_sym->LinkedScope->SupScopes()
    : concrete_sym->GenericConstraints | genex::views::transform([&](auto const &constraint) {
      return constraints_owner_scope.GetTypeSymbol(constraint.get())->LinkedScope;
    }) | genex::to<Vec>();
  sup_info.EmplaceBack(concrete_sym->FqName(), &concrete_scope);
  sup_info.AppendRange(type_members::SuperClassNames(sup_scopes));

  // Compare each constraint against the concrete type and its supertypes.
  for (auto const &constraint : constraints) {
    // Todo: document thread safety here.
    if (constraint->LastTypePart()->Name == THREAD_SAFE->LastTypePart()->Name
      and TypeEq(*constraint, *THREAD_SAFE, constraints_owner_scope, constraints_owner_scope)) {
      if (concrete_sym->IsThreadSafe()) { continue; }
      return constraint.get();
    }

    auto matched = false;
    for (auto const &[sup_type, sup_scope] : sup_info) {
      matched = TypeEq(*constraint, *sup_type, constraints_owner_scope, *sup_scope);
      if (matched) { break; }
    }

    // If any constraint is not met, return it so the caller can decide whether to raise an error.
    if (not matched) { return constraint.get(); }
  }

  // All constraints are satisfied.
  return nullptr;
}

auto spp::analyse::utils::type_compare::VariantMemberTypes(
  TypeAst const &type,
  Scope const &scope)
  -> Vec<Shared<TypeAst>> {
  // A written variant keeps its members as written, only a nested variant flattened into them - this is what a
  // variant's own name is rebuilt from ("Str or S32 or Str" as "Str or S32"). A type not written as a variant
  // ("Opt[S32]") is its variant's members, named as their symbols name them.
  auto const *const variants_arg = type.LastTypePart()->GnArgGroup->At("Variants");
  if (variants_arg != nullptr and variants_arg->TypeVal != nullptr) {
    return FlattenVariants(*variants_arg->TypeVal, scope)
      | genex::views::transform([](auto const &x) { return x.first; })
      | genex::to<Vec>();
  }
  return VariantMembers(TypeRef::Of(type, scope), scope)
    | genex::views::transform([](auto const &x) { return AsType(x); })
    | genex::to<Vec>();
}

auto spp::analyse::utils::type_compare::VariantMembers(
  TypeRef const &ref,
  Scope const &scope)
  -> Vec<TypeRef> {
  auto *const sym = ref.IsNever ? nullptr : CanonicalSym(ref.Sym);
  if (sym == nullptr) { return {}; }
  auto const *const variants = SymArgGroup(*sym).At("Variants");
  if (variants == nullptr or variants->TypeVal == nullptr) { return {}; }
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
    else if (s->Alias != nullptr and s->Alias->Resolved != nullptr) {
      if (auto *const target = scope.GetTypeSymbol(s->Alias->Resolved->WithoutGenerics().get()); target != nullptr) {
        next = target;
      }
    }
    if (next == s) { break; }
    s = next;
  }
  return s->InstanceOf != nullptr ? s->InstanceOf : s;
}

auto spp::analyse::utils::type_compare::IsTemplate(
  TypeSymbol const &sym,
  TypeAst const &tmpl,
  Scope const &scope)
  -> bool {
  auto const *tmpl_sym = scope.GetTypeSymbol(&tmpl);
  return tmpl_sym != nullptr and TemplateOf(sym, scope) == TemplateOf(*tmpl_sym, scope);
}
