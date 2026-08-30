module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_compare;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.utils.algorithms;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.strings;
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
      scopes::TypeSymbol *Pack;
    };

    auto ConstraintEq(
      Vec<Shared<asts::TypeAst>> const &constraints,
      asts::TypeAst const &type,
      scopes::Scope const &constraint_scope,
      scopes::Scope const &type_scope)
      -> bool {
      // If there are no constraints, then the match is default true,
      // because there are no restrictions on the "type" that can
      // possibly be checked for.
      if (constraints.IsEmpty()) { return true; }

      // Check that all the constraints are satisfied. Wraps the call
      // to the generic constraint enforcement (this function mainly
      // exists for the naming uniformity in type equality).
      return EnforceGenericConstraintsOneArg(
        constraints, type, constraint_scope, type_scope) == nullptr;
    }

    /**
     * Line up the written generic arguments of two types.
     * @param proto The prototype both lists were written for, whose parameters say whether it is variadic at all.
     * @param lhs_args The left-hand-side's written arguments.
     * @param rhs_args The right-hand-side's written arguments, the side a pack may be written on.
     * @param rhs_scope The scope the right-hand-side's arguments are named in.
     */
    auto MatchArgListArity(
      asts::ClassPrototypeAst const *proto,
      Vec<Unique<asts::GenericArgumentAst>> const &lhs_args,
      Vec<Unique<asts::GenericArgumentAst>> const &rhs_args,
      scopes::Scope const &rhs_scope)
      -> ArgListArity {
      //
      using namespace spp::asts;

      // The trailing argument is a pack only when it names a
      // variadic parameter that is still unbound.
      auto *pack = static_cast<scopes::TypeSymbol*>(nullptr);
      if (not rhs_args.IsEmpty()) {
        if (auto const *last = rhs_args.Back()->To<GenericArgumentTypeAst>(); last != nullptr) {
          const auto sym = rhs_scope.GetTypeSymbol(last->Val->WithoutGenerics().get(), false);
          if (sym != nullptr and sym->IsGeneric and sym->IsVariadic) { pack = sym; }
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
      scopes::TypeSymbol const &pack,
      Vec<Unique<asts::GenericArgumentAst>> const &lhs_args,
      const std::size_t fixed_len,
      scopes::Scope const &pack_scope,
      scopes::Scope const &arg_scope)
      -> bool {
      //
      if (pack.GenericConstraints.IsEmpty()) { return true; }

      for (auto i = fixed_len; i < lhs_args.Len(); ++i) {
        auto const *type_arg = lhs_args[i]->To<asts::GenericArgumentTypeAst>();
        if (type_arg == nullptr) { continue; }
        if (not ConstraintEq(pack.GenericConstraints, *type_arg->Val, pack_scope, arg_scope)) { return false; }
      }
      return true;
    }

    /**
     * Check whether a value of @p type can be held by the variant @p variant_type, which is how a variant accepts
     * anything other than itself. There are two ways in: @p type is one of the variant's members (@c {Some[T]} into a
     * @c {Opt[T]}), or @p type is itself a variant whose members are all members of this one (@c {Str or S32} into a
     * @c {Str or S32 or Bool}), because whichever member the narrower one holds, the wider one has room for it. An
     * overlap is not enough, as the members that are not shared would have nowhere to go.
     *
     * A type with no members is not a variant, so it never matches here; the caller falls back to comparing the two
     * types structurally, which is also what happens when the members do not line up.
     * @param variant_type The variant type being matched into.
     * @param type The type being matched, either a member or a narrower variant.
     * @param variant_scope The scope of the variant type.
     * @param type_scope The scope of the type being matched.
     * @return If a value of @p type can be held by @p variant_type.
     */
    auto TypeVariantEq(
      asts::TypeAst const &variant_type,
      asts::TypeAst const &type,
      scopes::Scope const &variant_scope,
      scopes::Scope const &type_scope)
      -> bool {
      // Get the members of the variant. If there are no members.
      // ie Var has no generics (shouldn't be possible), then
      // return false - impossible to match against.
      const auto variant_member_types = DedupVariableInnerTypes(variant_type, variant_scope);
      if (variant_member_types.IsEmpty()) { return false; }

      // When comparing two variants, the wider one must be able
      // to accept all the types of the narrower one. For example,
      // "Str or Bool or S32" accepts "Str or Bool", but not the
      // other way around - "Bool" wouldn't be accepted.
      const auto type_member_types = DedupVariableInnerTypes(type, type_scope);
      if (not type_member_types.IsEmpty()) {
        return genex::all_of(type_member_types, [&](auto &&type_member_type) {
          return genex::any_of(variant_member_types, [&](auto &&variant_member_type) {
            return TypeEq(*variant_member_type, *type_member_type, variant_scope, type_scope, false);
          });
        });
      }

      // Otherwise, it's a single type being compared, which matches
      // when it is one of the members.
      return genex::any_of(variant_member_types, [&](auto &&variant_member_type) {
        return TypeEq(*variant_member_type, type, variant_scope, type_scope);
      });
    }

    /**
     * Check whether a function "mock" type (a @c $ type generated per function, which superimposes a
     * @c FunMov/FunMut/FunRef type for each of its overloads) matches a target function type. This is what
     * allows a plain function or method to be passed wherever a function type is expected. @c $ types are
     * only ever generated for this purpose.
     * @param mock_type The @c $ mock type (the function/method reference).
     * @param func_type The target function type (@c FunMov/FunMut/FunRef) to match against.
     * @param mock_scope The scope of the mock type.
     * @param func_scope The scope of the target function type.
     * @return If any of the mock's superimposed function types is equal to the target function type.
     */
    auto TypeFuncEq(
      asts::TypeAst const &mock_type,
      asts::TypeAst const &func_type,
      scopes::Scope const &mock_scope,
      scopes::Scope const &func_scope)
      -> bool {
      // A "$" mock type is generated per function and superimposes
      // a function type for each of its overloads (and, because super
      // types are transitive, the whole FunMov/FunMut/FunRef hierarchy
      // above each). It matches the target function type if any of
      // those superimposed function types is equal to the target.
      const auto mock_sym = mock_scope.GetTypeSymbol(mock_type.WithoutConvention().get());
      if (mock_sym == nullptr) { return false; }

      for (auto const &sup_type : mock_sym->LinkedScope->SupTypes()) {
        if (type_predicates::IsTypeFunc(*sup_type, mock_scope) and
          TypeEq(*sup_type, func_type, mock_scope, func_scope)) {
          return true;
        }
      }
      return false;
    }
  }
}

auto spp::analyse::utils::type_compare::ConventionEq(
  asts::TypeAst const &lhs_type,
  asts::TypeAst const &rhs_type)
  -> bool {
  // Extract the conventions from the two types. These are
  // the asts that will get compared.
  const auto lhs_conv = lhs_type.GetConvention();
  const auto rhs_conv = rhs_type.GetConvention();

  // Quick exits based on existence of conventions. Only 2
  // no-conventions are a match, otherwise one existing and
  // the other not existing is a mismatch.
  if (lhs_conv == nullptr and rhs_conv == nullptr) { return true; }
  if (lhs_conv == nullptr and rhs_conv != nullptr) { return false; }
  if (lhs_conv != nullptr and rhs_conv == nullptr) { return false; }

  // If the conventions are not equal, return false, but
  // allow "&mut" (rhs) to coerce to "&" (lhs). This allows for
  // mutable references to be moved to functions that accept
  // immutable references, without re-borrowing.
  if (*lhs_conv != rhs_conv) {
    const auto is_lhs_mut = *lhs_conv == asts::ConventionTag::MUT;
    const auto is_rhs_ref = *rhs_conv == asts::ConventionTag::REF;
    return not(is_lhs_mut and is_rhs_ref);
  }

  // No other conditions have been met, so the conventions
  // must match at this point.
  return true;
}

auto spp::analyse::utils::type_compare::TypeEq(
  asts::TypeAst const &lhs_type,
  asts::TypeAst const &rhs_type,
  scopes::Scope const &lhs_scope,
  scopes::Scope const &rhs_scope,
  const bool check_variant)
  -> bool {
  // Use an identity fast path: the same type node is equal to
  // itself, skipping the strip + triple symbol lookup.
  if (&lhs_type == &rhs_type) { return true; }

  // Special case for the "!" and "Self" type; the "!" type on
  // the rhs is always considered a match, where-as if it's on
  // the lhs, the rhs must also be "!". Self types match based
  // on same, for implementation vs base matching on override
  // signature-type checking.
  if (rhs_type.IsNeverType()) { return true; }
  if (lhs_type.IsNeverType()) { return rhs_type.IsNeverType(); }
  if (lhs_type.IsSelfType() and rhs_type.IsSelfType()) { return true; }

  // Strip the generics from the types. This allows for the base
  // types to be retrieved and compared in their respective scopes.
  const auto stripped_lhs = lhs_type.WithoutGenerics();
  const auto stripped_rhs = rhs_type.WithoutGenerics();

  // Get the non-generic symbols. For the "Self" types, we reverse
  // the scopes, so that we get "Self" in the opposite scope, and
  // compare it to the type from that scope.
  auto stripped_lhs_sym = (lhs_type.IsSelfType() ? rhs_scope : lhs_scope).
    GetTypeSymbol(stripped_lhs.get(), false);
  auto stripped_rhs_sym = (rhs_type.IsSelfType() ? lhs_scope : rhs_scope).
    GetTypeSymbol(stripped_rhs.get(), false);

  // A "Self" symbol links to the class it stands for but carries no prototype of its own, and the prototype is what
  // the comparison below is on. Two "Self" types are already equal by the check above, so what is left is "Self"
  // against a written type - an instantiated body returning "Self" from a function whose return type resolved to the
  // concrete class, say - and that only matches once "Self" is followed through to the class it names.
  //
  // Only worth doing when the other side does name a class. Against a symbol that carries no prototype either (an
  // unbound generic parameter, most of all) the two match precisely by both being prototype-less, which is what lets
  // a method written in terms of "Self" register as overriding an abstract one; resolving would break that.
  const auto resolve_self_sym = [](scopes::TypeSymbol *const sym, scopes::TypeSymbol *const other) {
    return other != nullptr and other->Type != nullptr and sym != nullptr ? sym->AsClassSymbol() : sym;
  };
  if (lhs_type.IsSelfType()) { stripped_lhs_sym = resolve_self_sym(stripped_lhs_sym, stripped_rhs_sym); }
  if (rhs_type.IsSelfType()) { stripped_rhs_sym = resolve_self_sym(stripped_rhs_sym, stripped_lhs_sym); }
  const auto lhs_sym = lhs_scope.GetTypeSymbol(&lhs_type);

  // If the left-hand-side is a "Variant" type, check the member
  // types first; "Str or Bool" should accept "Str", and also
  // "Str or Bool or S32" should accept "Str or S32" (subset).
  if (check_variant and TypeVariantEq(lhs_type, rhs_type, lhs_scope, rhs_scope)) { return true; }
  if (not ConventionEq(lhs_type, rhs_type)) { return false; }

  // Todo: document this.
  if (stripped_lhs_sym != nullptr and stripped_rhs_sym != nullptr
    and stripped_lhs_sym->IsGeneric and stripped_rhs_sym->IsGeneric
    and (stripped_lhs_sym->Type == nullptr or stripped_rhs_sym->Type == nullptr)
    and *stripped_lhs_sym->Name == *stripped_rhs_sym->Name) {
    return true;
  }

  // If the stripped types are not equal, check function-mock and
  // forwarding compatibility before returning false. A "$" mock type
  // is a function value: match it structurally against the target
  // function type via its superimposed overload types ($ types are
  // only ever generated for this purpose).
  if (stripped_lhs_sym == nullptr or stripped_rhs_sym == nullptr
    or stripped_lhs_sym->Type != stripped_rhs_sym->Type) {
    if (lhs_type.IsCompilerGeneratedType()) { return TypeFuncEq(lhs_type, rhs_type, lhs_scope, rhs_scope); }
    if (rhs_type.IsCompilerGeneratedType()) { return TypeFuncEq(rhs_type, lhs_type, rhs_scope, lhs_scope); }
    return TypeFwdEq(rhs_type, lhs_type, rhs_scope, lhs_scope);
  }

  // Next the generics must be handled. Firstly get the generics
  // for both types, and then do a special variadic length check.
  auto &lhs_generics = lhs_type.LastTypePart()->GnArgGroup->Args;
  auto &rhs_generics = rhs_type.LastTypePart()->GnArgGroup->Args;

  // Line the two argument lists up. A variadic type ("Tup") takes any number of arguments, so a longer list must not
  // be cut off against a shorter one and assumed equal; a trailing argument naming a variadic parameter is the one
  // that legitimately covers a remainder.
  const auto arity = MatchArgListArity(lhs_sym->Type, lhs_generics, rhs_generics, rhs_scope);
  if (not arity.Compatible) { return false; }

  // Ensure each generic argument is symbolically equal to the
  // other. Split on the type/comp argument type, and we can
  // do it positionally because analysis orders the args against
  // the params.
  for (auto i = 0uz; i < arity.FixedLen; ++i) {
    auto const &lhs_generic = lhs_generics[i];
    auto const &rhs_generic = rhs_generics[i];
    if (lhs_generic->To<asts::GenericArgumentTypeAst>()) {
      const auto lhs_generic_part = lhs_generic->To<asts::GenericArgumentTypeAst>();
      const auto rhs_generic_part = rhs_generic->To<asts::GenericArgumentTypeAst>();
      if (not TypeEq(*lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope)) { return false; }
    }
    else {
      const auto lhs_generic_part = lhs_generic->To<asts::GenericArgumentCompAst>();
      const auto rhs_generic_part = rhs_generic->To<asts::GenericArgumentCompAst>();
      if (not TypeEq(*lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope)) { return false; }
    }
  }

  // If all the generic arguments are symbolically equal, return
  // true.
  return true;
}

auto spp::analyse::utils::type_compare::TypeEq(
  asts::ExpressionAst const &lhs_expr,
  asts::ExpressionAst const &rhs_expr,
  scopes::Scope const &,
  scopes::Scope const &)
  -> bool {
  // Simple equality between the expressions. As there are
  // references, not pointers, the inner values of the asts
  // get compared (see the ExpressionAst equality methods).
  return lhs_expr == rhs_expr;
}

auto spp::analyse::utils::type_compare::TypeFwdEq(
  asts::TypeAst const &arg_type,
  asts::TypeAst const &param_type,
  scopes::Scope const &arg_scope,
  scopes::Scope const &param_scope)
  -> bool {
  // The type-forwarding matcher allows for a param of "&StrView"
  // to be matched with an argument type of "&Str", because "Str"
  // forwards to "&StrView" under "FwdRef[T=StrView]".
  using asts::generate::common_types_precompiled::FWD_REF;
  using asts::generate::common_types_precompiled::FWD_MUT;

  // Ensure that the conventions match between the two types
  // exactly (ie "Str" -> "&StrView" doesn't work, where-as "&Str"
  // -> "&StrView" is fine. First part checks for a no-convention.
  const auto arg_conv = arg_type.GetConvention();
  const auto param_conv = param_type.GetConvention();
  if (arg_conv == nullptr or param_conv == nullptr) { return false; }

  // Determine if we are targeting a forwarding ref or mut variation.
  // This is the second part of the convention check and ensures that
  // either both are an immutable or a mutable borrow.
  const auto is_both_ref = (*arg_conv == asts::ConventionTag::REF) and (*param_conv == asts::ConventionTag::REF);
  const auto is_both_mut = (*arg_conv == asts::ConventionTag::MUT) and (*param_conv == asts::ConventionTag::MUT);
  if (not is_both_ref and not is_both_mut) { return false; }

  // Generic types won't forward, so ignore them here.
  // Todo: maybe allow via constraints at some point.
  const auto &fwd_target = is_both_ref ? FWD_REF : FWD_MUT;
  const auto arg_bare = arg_type.WithoutConvention();
  const auto arg_bare_sym = arg_scope.GetTypeSymbol(arg_bare.get());
  if (arg_bare_sym == nullptr or arg_bare_sym->IsGeneric) { return false; }

  // An argument that already names the parameter's own class is
  // not forwarded to it. The loop below reaches the parameter's
  // type again by going around the cycle - "NonNull" forwards to
  // "&T", and "&mut NonNull[T]" against a "&mut NonNull[T]"
  // parameter comes back a match - and the caller then rewrites
  // the argument to "x.fwd_ref()", so what the callee is handed
  // is the pointee instead of the slot the argument named. Also
  // messes up the memory analysis as it uses the non-symbolic
  // function call otherwise.
  const auto param_bare = param_type.WithoutConvention();
  const auto param_bare_sym = param_scope.GetTypeSymbol(param_bare.get());
  if (param_bare_sym != nullptr and arg_bare_sym->LinkedScope != nullptr
    and arg_bare_sym->LinkedScope->NonGenericScope == (param_bare_sym->LinkedScope != nullptr
      ? param_bare_sym->LinkedScope->NonGenericScope
      : nullptr)) {
    return false;
  }

  // Get all the super types that we want to consider. This is
  // what we will search in for the `FwdXXX` types.
  auto sup_types = Vec{arg_bare};
  sup_types.AppendRange(arg_bare_sym->LinkedScope->SupTypes());

  // Check for a matching forwarding type, and compare to the
  // inner type of it (the forwarding target).
  // Todo: ensure only 1 forwarding superimposition is present for a given type.
  // Todo: probably ensure that the ref & mut both forward to the same type?
  for (auto const &sup_type : sup_types) {
    if (not TypeEq(*sup_type->WithoutGenerics(), *fwd_target, arg_scope, arg_scope, false)) { continue; }
    const auto inner_type = sup_type->LastTypePart()->GnArgGroup->TypeAt("T")->Val->WithConvention(
      asts::AstClone(param_type.GetConvention()));
    if (TypeEq(*inner_type, param_type, param_scope, param_scope)) { return true; }
  }

  // Otherwise, there is no forwarding match, so return false.
  return false;
}

auto spp::analyse::utils::type_compare::RelaxedTypeEq(
  asts::TypeAst const &lhs_type,
  asts::TypeAst const &rhs_type,
  scopes::Scope const &lhs_scope,
  scopes::Scope const &rhs_scope,
  GenericInferenceMap &generic_args,
  const bool check_variant,
  const bool check_constraints,
  const bool strict_generic_args) -> bool {
  // Todo: Make this left relaxed only and remove strict_generic_args?
  // Strip the generics from the types. This allows for the base
  // types to be retrieved and compared in their respective scopes.
  using asts::generate::common_types_precompiled::VAR;
  const auto stripped_lhs = mut_shared_cast(lhs_type.WithoutGenerics()->WithoutConvention());
  const auto stripped_rhs = mut_shared_cast(rhs_type.WithoutGenerics()->WithoutConvention());

  // If the right-hand-side is directly generic, then return a
  // match: "sup[T] T { ... }" matches all types. Record the generic
  // in the map too.
  const auto stripped_rhs_sym = rhs_scope.GetTypeSymbol(stripped_rhs.get());
  if (stripped_rhs_sym == nullptr) { return false; }
  if (stripped_rhs_sym->IsGeneric) {
    const auto t = static_shared_cast<asts::TypeIdentifierAst>(stripped_rhs);
    generic_args.insert({t, const_cast<asts::TypeAst*>(&lhs_type)});
    if (check_constraints and not ConstraintEq(stripped_rhs_sym->GenericConstraints, lhs_type, rhs_scope, lhs_scope)) {
      return false;
    }
    return true;
  }

  // TODO: Deliberately inverted for the param/arg checker. This will be
  //  removed once that type check is actually done properly.
  if (not ConventionEq(rhs_type, lhs_type)) { return false; }

  // The same as above, but for the left-hand-side: auto match on a
  // direct generic and record the mapping.
  const auto stripped_lhs_sym = lhs_scope.GetTypeSymbol(stripped_lhs.get());
  if (stripped_lhs_sym == nullptr) { return false; }
  if (stripped_lhs_sym->IsGeneric) {
    const auto t = static_shared_cast<asts::TypeIdentifierAst>(stripped_lhs);
    generic_args.insert({t, const_cast<asts::TypeAst*>(&rhs_type)});
    if (check_constraints and not ConstraintEq(stripped_lhs_sym->GenericConstraints, rhs_type, lhs_scope, rhs_scope)) {
      return false;
    }
    return true;
  }

  // If the right-hand-side is a "Variant" type, check the member
  // types first; "Str or Bool" should accept "Str", and also
  // "Str or Bool or S32" should accept "Str or S32" (subset).
  // Todo: on the failure of a variant match in "any_of", does the generic map need rolling back?
  // Todo: more advanced check like TypeEq?
  if (check_variant and TypeEq(*VAR, *stripped_rhs_sym->FqName()->WithoutGenerics(), rhs_scope, rhs_scope)) {
    auto rhs_composite_types = DedupVariableInnerTypes(*rhs_scope.GetTypeSymbol(&rhs_type)->FqName(), rhs_scope);
    if (genex::any_of(rhs_composite_types, [&](auto &&rhs_composite_type) {
      return RelaxedTypeEq(
        lhs_type, *rhs_composite_type, lhs_scope, rhs_scope, generic_args,
        true, check_constraints, strict_generic_args);
    })) {
      return true;
    }
  }

  // Next the generics must be handled. Firstly get the generics
  // for both types, and then do a special variadic length check.
  if (stripped_lhs_sym->Type != stripped_rhs_sym->Type) { return false; }
  auto &lhs_generics = lhs_type.LastTypePart()->GnArgGroup->Args;
  auto &rhs_generics = rhs_type.LastTypePart()->GnArgGroup->Args;

  // Line the two argument lists up, so that "Tup[T, U, V]" stops
  // matching a two element tuple, while a trailing argument naming
  // a variadic parameter ("sup [..Ts] Tup[Ts]") goes on covering a
  // list of any length.
  const auto arity = MatchArgListArity(
    stripped_lhs_sym->Type, lhs_generics, rhs_generics, rhs_scope);
  if (not arity.Compatible) { return false; }

  // Every element the pack swallows has to satisfy the pack's own
  // constraints, one at a time: "sup [..Ts: Copy] Tup[Ts]" says
  // every element is copyable, not that the tuple is.
  if (check_constraints and arity.Pack != nullptr
    and not PackConstraintsSatisfied(*arity.Pack, lhs_generics, arity.FixedLen, rhs_scope, lhs_scope)) {
    return false;
  }

  // Ensure each generic argument is symbolically equal to the
  // other. Split on the type/comp argument type, and we can
  // do it positionally because analysis orders the args against
  // the params.
  for (auto i = 0uz; i < arity.FixedLen; ++i) {
    auto const &lhs_generic = lhs_generics[i];
    auto const &rhs_generic = rhs_generics[i];
    if (const auto rhs_generic_part_t = rhs_generic->To<asts::GenericArgumentTypeAst>()) {
      const auto rhs_generic_part = rhs_generic_part_t;
      const auto lhs_generic_part = lhs_generic->ToUnchecked<asts::GenericArgumentTypeAst>();

      // Under "strict_generic_args", an argument that is still an unbound parameter is not the particular type
      // written opposite it. Only the arguments are held to this: the bare parameter itself, compared against a type
      // it is constrained by, has to keep matching, because that comparison is how a constraint's "sup" block is
      // attached to the parameter in the first place.
      if (strict_generic_args) {
        const auto lhs_arg_sym = lhs_scope.GetTypeSymbol(lhs_generic_part->Val->WithoutGenerics().get());
        const auto rhs_arg_sym = rhs_scope.GetTypeSymbol(rhs_generic_part->Val->WithoutGenerics().get());
        if (lhs_arg_sym != nullptr and lhs_arg_sym->IsGeneric and lhs_arg_sym->Type == nullptr
          and rhs_arg_sym != nullptr and not rhs_arg_sym->IsGeneric) { return false; }
      }

      if (not RelaxedTypeEq(
        *lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope, generic_args,
        check_variant, check_constraints, strict_generic_args)) { return false; }
    }
    else {
      // The comp arm resolves to the "ExpressionAst" overload, which compares values and has no flags to forward.
      const auto lhs_generic_part = lhs_generic->ToUnchecked<asts::GenericArgumentCompAst>();
      const auto rhs_generic_part = rhs_generic->ToUnchecked<asts::GenericArgumentCompAst>();
      if (not RelaxedTypeEq(
        *lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope, generic_args)) {
        return false;
      }
    }
  }

  // If all the generic arguments are symbolically equal, return
  // true.
  return true;
}

auto spp::analyse::utils::type_compare::RelaxedTypeEq(
  asts::ExpressionAst const &lhs_expr,
  asts::ExpressionAst const &rhs_expr,
  scopes::Scope const &,
  scopes::Scope const &,
  GenericInferenceMap &generic_args)
  -> bool {
  // Simple equality between the expressions, with generic matching.
  // Save generic mapping for identifier expressions on one side.
  if (const auto rhs_expr_as_identifier = rhs_expr.To<asts::IdentifierAst>()) {
    generic_args[asts::TypeIdentifierAst::FromIdentifier(*rhs_expr_as_identifier)] =
      const_cast<asts::ExpressionAst*>(&lhs_expr);
    return true;
  }
  return lhs_expr == rhs_expr;
}

auto spp::analyse::utils::type_compare::EnforceGenericConstraintsOneArg(
  Vec<Shared<asts::TypeAst>> const &constraints,
  asts::TypeAst const &concrete_type,
  scopes::Scope const &constraints_owner_scope,
  scopes::Scope const &concrete_scope)
  -> asts::TypeAst const* {
  // Note: concrete scope is where the type is being used; concrete_sym->LinkedScope is the scope of the type
  // definition.

  // Determine the concrete symbol, and if non-generic, add its scope.
  const auto concrete_sym = concrete_scope.GetTypeSymbol(&concrete_type);
  auto sup_info = Vec<Pair<Shared<asts::TypeAst>, scopes::Scope const*>>{};
  if (concrete_type.IsSelfType() and not concrete_sym->IsGeneric) {
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
  for (auto const *sup_scope : sup_scopes) {
    if (sup_scope->AstNode->To<asts::ClassPrototypeAst>() == nullptr) { continue; }
    const auto &sup_sym = sup_scope->TySym;
    sup_info.EmplaceBack(sup_sym->FqName(), sup_scope);
  }

  // Compare each constraint against the concrete type and its supertypes.
  for (auto const &constraint : constraints) {
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

auto spp::analyse::utils::type_compare::DedupVariableInnerTypes(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Vec<Shared<asts::TypeAst>> {
  // Extract the initial "Variant" group of member types (if possible ie raw variant).
  auto out = Vec<Shared<asts::TypeAst>>();
  auto variants_arg = type.LastTypePart()->GnArgGroup->TypeAt("Variants");
  auto resolved_type = Shared<asts::TypeAst>(nullptr);
  if (variants_arg == nullptr) {
    // Truly non-variant: return the empty list (no variant members).
    const auto type_sym = scope.GetTypeSymbol(&type);
    if (type_sym == nullptr) { return out; }
    resolved_type = type_sym->FqName();
    if (resolved_type == nullptr) { return out; }

    // Re-extract the variants from the true type.
    variants_arg = resolved_type->LastTypePart()->GnArgGroup->TypeAt("Variants");
  }

  // Guard against non variant types.
  if (variants_arg == nullptr or variants_arg->Val == nullptr) { return out; }

  // Add a member, unless a TypeEq-equal one is already present.
  auto add_unique = [&out, &scope](Shared<asts::TypeAst> const &member) {
    if (not genex::any_of(out, [&](auto x) { return TypeEq(*member, *x, scope, scope, false); })) {
      out.EmplaceBack(member);
    }
  };

  // Recursively search through the variant's member types, adding the unique ones.
  const auto &var_generic_args = variants_arg->Val->LastTypePart()->GnArgGroup;
  for (auto &&generic_arg : var_generic_args->GetTypeArgs()) {
    auto inner_types = DedupVariableInnerTypes(*generic_arg->Val, scope);
    if (inner_types.IsEmpty()) { add_unique(generic_arg->Val); }
    else {
      for (auto const &inner_type : inner_types) { add_unique(inner_type); }
    }
  }

  // Return the deduplicated list of types.
  return out;
}
