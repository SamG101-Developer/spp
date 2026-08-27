module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_utils;
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

namespace {
  auto IsTypeTry(
    spp::asts::TypeAst const &type,
    spp::analyse::scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::try::Try[Ok, Err]".
    using spp::asts::generate::common_types_precompiled::TRY;
    using spp::analyse::utils::type_utils::TypeEq;

    return TypeEq(*type.WithoutGenerics(), *TRY, scope, scope);
  }

  auto GetAttrTypes(
    const spp::asts::ClassPrototypeAst *cls_proto,
    const spp::analyse::scopes::Scope *cls_scope,
    spp::Vec<spp::Pair<spp::analyse::scopes::TypeSymbol*, spp::asts::ClassAttributeAst*>> &attr_symbols)
    -> void {
    // Get all attribute types, without recursion errors (this will
    // be handled elsewhere, so assume it has been checked already).
    for (auto const &member : cls_proto->Impl->Members
         | genex::views::ptr
         | genex::views::cast_dynamic<spp::asts::ClassAttributeAst*>) {
      auto type_sym = cls_scope->GetTypeSymbol(member->Type.get());
      if (genex::contains(attr_symbols, type_sym, [](auto &&x) { return x.first; })) { continue; }
      if (type_sym->IsGeneric) { continue; }

      attr_symbols.EmplaceBack(type_sym, member);
      GetAttrTypes(type_sym->Type, type_sym->LinkedScope, attr_symbols);
    }
  }

  /** How two written generic argument lists line up, for the type they were both written for. */
  struct ArgListArity {
    /** Whether the two lengths can describe the same type at all. */
    bool Compatible;

    /** How many leading arguments are compared one against one; anything past this is the pack. */
    std::size_t FixedLen;

    /** The variadic parameter the right-hand-side's trailing argument names, if it names one. */
    spp::analyse::scopes::TypeSymbol *Pack;
  };

  /**
   * Line up the written generic arguments of two types.
   * @param proto The prototype both lists were written for, whose parameters say whether it is variadic at all.
   * @param lhs_args The left-hand-side's written arguments.
   * @param rhs_args The right-hand-side's written arguments, the side a pack may be written on.
   * @param rhs_scope The scope the right-hand-side's arguments are named in.
   */
  auto MatchArgListArity(
    spp::asts::ClassPrototypeAst const *proto,
    spp::Vec<spp::Unique<spp::asts::GenericArgumentAst>> const &lhs_args,
    spp::Vec<spp::Unique<spp::asts::GenericArgumentAst>> const &rhs_args,
    spp::analyse::scopes::Scope const &rhs_scope)
    -> ArgListArity {
    //
    using namespace spp::asts;

    // The trailing argument is a pack only when it names a
    // variadic parameter that is still unbound.
    auto *pack = static_cast<spp::analyse::scopes::TypeSymbol*>(nullptr);
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
    spp::analyse::scopes::TypeSymbol const &pack,
    spp::Vec<spp::Unique<spp::asts::GenericArgumentAst>> const &lhs_args,
    const std::size_t fixed_len,
    spp::analyse::scopes::Scope const &pack_scope,
    spp::analyse::scopes::Scope const &arg_scope)
    -> bool {
    //
    using spp::analyse::utils::type_utils::ConstraintEq;
    if (pack.GenericConstraints.IsEmpty()) { return true; }

    for (auto i = fixed_len; i < lhs_args.Len(); ++i) {
      auto const *type_arg = lhs_args[i]->To<spp::asts::GenericArgumentTypeAst>();
      if (type_arg == nullptr) { continue; }
      if (not ConstraintEq(pack.GenericConstraints, *type_arg->Val, pack_scope, arg_scope)) { return false; }
    }
    return true;
  }
}

auto spp::analyse::utils::type_utils::ConventionEq(
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

auto spp::analyse::utils::type_utils::ConstraintEq(
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

auto spp::analyse::utils::type_utils::TypeEq(
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
  if (stripped_lhs_sym->Type != stripped_rhs_sym->Type) {
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

auto spp::analyse::utils::type_utils::TypeEq(
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

auto spp::analyse::utils::type_utils::TypeVariantEq(
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

auto spp::analyse::utils::type_utils::TypeFwdEq(
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

auto spp::analyse::utils::type_utils::TypeFuncEq(
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
    if (IsTypeFunc(*sup_type, mock_scope) and TypeEq(*sup_type, func_type, mock_scope, func_scope)) {
      return true;
    }
  }
  return false;
}

auto spp::analyse::utils::type_utils::RelaxedTypeEq(
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
      return RelaxedTypeEq(lhs_type, *rhs_composite_type, lhs_scope, rhs_scope, generic_args);
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

auto spp::analyse::utils::type_utils::RelaxedTypeEq(
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

auto spp::analyse::utils::type_utils::IsTypeCompTimeIndexable(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // The only two types that can be indexed at compile time are the
  // tuple type, and the array type.
  return
    IsTypeTup(*type.WithoutGenerics(), scope) or IsTypeArr(*type.WithoutGenerics(), scope);
}

auto spp::analyse::utils::type_utils::IsTypeArr(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::array::Arr[T, n]". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::ARR;
  return TypeEq(*type.WithoutGenerics(), *ARR, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeTup(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "Tup::Tup[Ts...]". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::TUP;
  return TypeEq(*type.WithoutGenerics(), *TUP, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTupSymbol(
  scopes::TypeSymbol const &sym)
  -> bool {
  // Compared against the precompiled name rather than through a scope, because the symbol's own qualified name is
  // already the answer: an alias for the tuple resolves to "std::tuple::Tup" just as the type itself does.
  using asts::generate::common_types_precompiled::TUP;
  const auto as_unary = dynamic_shared_cast<asts::TypeUnaryExpressionAst>(sym.FqName()->WithoutGenerics());
  return as_unary != nullptr and *as_unary == *TUP->ToUnchecked<asts::TypeUnaryExpressionAst>();
}

auto spp::analyse::utils::type_utils::IsTypeVariant(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::variant::Variant[Ts...]". This
  // only considers the type directly, not any supertypes. It does
  // a "remove convention" first. Todo: Conv for others?
  using asts::generate::common_types_precompiled::VAR;
  return TypeEq(*type.WithoutConvention()->WithoutGenerics(), *VAR, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeBool(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::bool::Bool". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::BOOL;
  return TypeEq(type, *BOOL, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeGen(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::generator::Gen[T]" or
  // "std::generator::GenOnce[T]". This only considers the
  // type directly, not any supertypes.
  using asts::generate::common_types_precompiled::GEN;
  using asts::generate::common_types_precompiled::GEN_ONCE;

  return
    TypeEq(*type.WithoutGenerics(), *GEN, scope, scope) or
    TypeEq(*type.WithoutGenerics(), *GEN_ONCE, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeVoid(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::void::Void". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::VOID;
  return TypeEq(type, *VOID, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeNever(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::never::Never". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::NEVER;
  return TypeEq(type, *NEVER, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeSelf(
  asts::TypeAst const &type)
  -> bool {
  // Check for a string match to "Self".
  const auto type_identifier = type.To<asts::TypeIdentifierAst>();
  return type_identifier != nullptr and type_identifier->Name == "Self";
}

auto spp::analyse::utils::type_utils::IsTypeFunc(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against one of the following three targets:
  // `std::function::FunRef|FunMut|FunMov[Args, Out]`. This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::FUN_MOV;
  using asts::generate::common_types_precompiled::FUN_MUT;
  using asts::generate::common_types_precompiled::FUN_REF;
  return
    TypeEq(*type.WithoutGenerics(), *FUN_MOV, scope, scope) or
    TypeEq(*type.WithoutGenerics(), *FUN_MUT, scope, scope) or
    TypeEq(*type.WithoutGenerics(), *FUN_REF, scope, scope);
}

auto spp::analyse::utils::type_utils::GetSuperimposedFatPointerFieldCount(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> std::size_t {
  const auto type_sym = scope.GetTypeSymbol(&type);
  if (type_sym == nullptr or type_sym->LinkedScope == nullptr) { return 0uz; }

  // "Gen"/"GenOnce" lower to a single opaque llvm coroutine handle
  // (the "llvm.coro.begin" result) rather than a true 2-pointer fat
  // pointer - only the "FunXXX" family is a { fn_ptr, env_ptr } pair.
  for (auto const &sup_type : type_sym->LinkedScope->SupTypes()) {
    if (IsTypeGen(*sup_type, *type_sym->LinkedScope)) { return 1uz; }
    if (IsTypeFunc(*sup_type, *type_sym->LinkedScope)) { return 2uz; }
  }
  return 0uz;
}

auto spp::analyse::utils::type_utils::IsTypeFullyConcrete(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Only a name that positively resolves to an unbound parameter counts against the type. Such a symbol is found but
  // carries no prototype - it is linked to the dummy scope "GenericParameterTypeAst::Stage2_GenTopLvlScopes" makes for
  // it - where a parameter bound to a real type carries that type's prototype.
  //
  // A name that resolves to nothing at all is a different situation and is deliberately not treated as a parameter: a
  // nested argument is looked up in the instantiation's own scope, which need not have every type its arguments were
  // written in terms of in view, and reading "not found" as "still generic" would refuse perfectly good instantiations.
  const auto stripped = type.WithoutGenerics()->WithoutConvention();
  const auto sym = scope.GetTypeSymbol(stripped.get());
  if (type.IsSelfType()) { return false; }
  if (sym != nullptr and sym->Type == nullptr) { return false; }

  // Then every argument, recursively. Recursion terminates because a written type is a finite tree; it is the
  // arguments that carry the parameters, and a type like "NonNull[T=T]" is only distinguishable from "NonNull[T=U8]"
  // by looking at them.
  for (auto const &gn_arg : type.LastTypePart()->GnArgGroup->Args) {
    if (const auto type_arg = gn_arg->To<asts::GenericArgumentTypeAst>(); type_arg != nullptr) {
      if (not IsTypeFullyConcrete(*type_arg->Val, scope)) { return false; }
      continue;
    }

    // A comp argument still written as a name is a parameter rather than a value: "SizedInteger[w=w]" is the template
    // and "SizedInteger[w=32]" is the instantiation, and only the second has a width to lower to (see the
    // "kSizedIntegerParts" case in "RegisterLlvmTypeInfo", which gives up on anything that is not a literal). A name
    // that stood for a value would have been rewritten to that value when the instantiation was built.
    if (const auto comp_arg = gn_arg->To<asts::GenericArgumentCompAst>(); comp_arg != nullptr) {
      if (comp_arg->Val->To<asts::IdentifierAst>() != nullptr) { return false; }
    }
  }
  return true;
}

auto spp::analyse::utils::type_utils::IsTypeRecursive(
  asts::ClassPrototypeAst const &type,
  scopes::ScopeManager const &sm)
  -> Shared<asts::TypeAst> {
  // Get the attribute types recursively from the class prototype,
  // and check for a match with the class prototype. Use the source
  // type as this function is used for error reporting exclusively.
  auto attr_info = Vec<Pair<scopes::TypeSymbol*, asts::ClassAttributeAst*>>{};
  GetAttrTypes(&type, sm.CurrentScope, attr_info);
  for (auto const &[attr_type_sym, attr_ast] : attr_info) {
    if (attr_type_sym == type.GetClsSym().get()) {
      return attr_ast->Source.OriginalType;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::type_utils::IsTypeBorrowed(
  asts::TypeAst const &type,
  scopes::ScopeManager const &sm,
  const bool deep)
  -> bool {
  // Check that either this type, or any inner types for variants,
  // are "&" or "&mut". Start with short-circuits on the type given,
  // which might contain an "&"/"&mut" unary operator.
  using asts::generate::common_types_precompiled::VAR;
  if (type.GetConvention() != nullptr) { return true; }
  if (type.IsSelfType()) { return false; }

  // Check the inner types for variant types. Reuse this function
  // recursively to reach any depth type, and check for a possible
  // borrow.
  if (deep and TypeEq(*type.WithoutGenerics(), *VAR, *sm.CurrentScope, *sm.CurrentScope, false)) {
    for (auto const &inner_type : DedupVariableInnerTypes(type, *sm.CurrentScope)) {
      if (IsTypeBorrowed(*inner_type, sm)) { return true; }
    }
  }

  // No borrowing of any nature discovered => non borrowable type.
  // Checked all depths.
  return false;
}

auto spp::analyse::utils::type_utils::IsIndexWithinBound(
  const std::size_t index,
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Pair<bool, std::size_t> {
  // For tuples, count the number of generic arguments. This is the
  // number of arguments in the tuple. amd the upper bound.
  // Todo: What about variadic tuples? Per-proto analysis catches this?
  //  Add some unit tests to check.
  using errors::SppInternalCompilerError;
  if (IsTypeTup(type, scope)) {
    const auto elems = type.LastTypePart()->GnArgGroup->Args.Len();
    return {index < elems, elems};
  }

  // For arrays, check the size argument. This is the compile time
  // generic argument "n" that is always known / resolved.
  if (IsTypeArr(type, scope)) {
    const auto size_arg = type.LastTypePart()->GnArgGroup->CompAt("n");
    const auto size_arg_cast = size_arg->Val->To<asts::IntegerLiteralAst>();
    const auto elems = std::stoul(size_arg_cast->Val->TokenData);
    return {index < elems, elems};
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(type, err_msg));
}

auto spp::analyse::utils::type_utils::GetNthTypeOfIndexableType(
  const std::size_t index,
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Shared<asts::TypeAst> {
  // For tuples, return the nth generic argument. This can be
  // different per element.
  using errors::SppInternalCompilerError;
  if (IsTypeTup(type, scope)) {
    return type.LastTypePart()->GnArgGroup->GetTypeArgs()[index]->Val;
  }

  // For arrays, return the element type. This is always the same
  // per element.
  if (IsTypeArr(type, scope)) {
    return type.LastTypePart()->GnArgGroup->GetTypeArgs()[0]->Val;
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(type, err_msg));
}

auto spp::analyse::utils::type_utils::GetFunctionalType(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Shared<const asts::TypeAst> {
  //
  const auto type_sym = scope.GetTypeSymbol(&type);

  // Callable quick-fix to use the constrained callable type
  // rather than the genuine one for memory-analysis reasons;
  // constraint of FunMov but passed as FunMut needs to still
  // use the FunMov overload.
  for (auto const &constraint : type_sym->GenericConstraints) {
    if (IsTypeFunc(*constraint, scope)) { return constraint; }
  }

  // Check the type itself and all its supertypes (a type
  // superimposing a function type is also callable).
  auto sup_types = Vec{type.shared_from_this()};
  sup_types.AppendRange(type_sym->LinkedScope->SupTypes());
  for (auto const &sup_type : sup_types) {
    if (IsTypeFunc(*sup_type, scope)) { return sup_type; }
  }

  return nullptr;
}

auto spp::analyse::utils::type_utils::GetGenAndYieldTypes(
  asts::TypeAst const &type,
  scopes::Scope const &scope,
  asts::ExpressionAst const &expr,
  StrView what,
  const bool raise)
  -> Tup<Shared<const asts::TypeAst>, Shared<asts::TypeAst>, bool> {
  //
  using asts::generate::common_types_precompiled::GEN_ONCE;
  using errors::SppExpressionNotGeneratorError;
  using errors::SppExpressionAmbiguousGeneratorError;

  // Generic types are not generators, so raise an error.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests.
  const auto type_sym = scope.GetTypeSymbol(&type);

  // Discover the supertypes and add the current type to it.
  auto sup_types = Vec{type.shared_from_this()};
  sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

  // Search through the supertypes for a direct generator type.
  // Simple comparison check against the Gen and GenOnce types.
  const auto generator_type_candidates = sup_types
    | genex::views::filter([&](auto const &sup_type) { return IsTypeGen(*sup_type, scope); })
    | genex::to<Vec>();

  // If there are no Gen or GenOnce super types, then the
  // generator and yield type cannot be obtained, so either
  // throw an error or return nullptr.
  if (generator_type_candidates.IsEmpty()) {
    RaiseIf<SppExpressionNotGeneratorError>(
      raise, {&scope}, ERR_ARGS(expr, type, what));
    return {nullptr, nullptr, false};
  }

  // If there are more than 1 Gen or GenOnce super types, then
  // the generator and yield types would be ambiguous, so either
  // throw an error or return nullptr.
  if (generator_type_candidates.Len() > 1) {
    RaiseIf<SppExpressionAmbiguousGeneratorError>(
      raise, {&scope}, ERR_ARGS(expr, type, what));
    return {nullptr, nullptr, false};
  }

  // Extract the generator and yield type from the candidates.
  // Accessing [0] is safe as we have already done the validation
  // beforehand.
  auto generator_type = generator_type_candidates[0];
  auto yield_type = generator_type->LastTypePart()->GnArgGroup->TypeAt("Yield")->Val;
  auto is_once = TypeEq(
    *GEN_ONCE, *generator_type->WithoutGenerics(), scope, scope);

  // Return all the information about the generator type.
  return {generator_type, yield_type, is_once};
}

auto spp::analyse::utils::type_utils::GetTryType(
  asts::TypeAst const &type,
  asts::ExpressionAst const &expr,
  scopes::ScopeManager const &sm,
  StrView what,
  const bool raise)
  -> Shared<const asts::TypeAst> {
  // Generic types are not Try types, so return nullptr.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests.
  const auto type_sym = sm.CurrentScope->GetTypeSymbol(&type);
  if (type_sym->IsGeneric) { return nullptr; }

  // Discover the supertypes and add the current type to it.
  auto sup_types = Vec{type.shared_from_this()};
  sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

  // Search through the supertypes for a direct try type.
  // Simple comparison check against the Try types.
  const auto try_type_candidates = sup_types
    | genex::views::filter([&sm](auto &&sup_type) { return IsTypeTry(*sup_type, *sm.CurrentScope); })
    | genex::to<Vec>();

  // If there are no Try super types, then the try type cannot
  // be obtained, so either throw an error or return nullptr.
  if (try_type_candidates.IsEmpty()) {
    RaiseIf<errors::SppExpressionNotTryError>(
      raise, {sm.CurrentScope}, ERR_ARGS(expr, type));
    return nullptr;
  }

  // If there are more than 1 Try super types, then the Try
  // type would be ambiguous, so either throw an error or
  // return nullptr.
  if (try_type_candidates.Len() > 1) {
    RaiseIf<errors::SppExpressionAmbiguousTryError>(
      raise, {sm.CurrentScope}, ERR_ARGS(expr, type, what));
    return nullptr;
  }

  // Extract the Try type and return it.
  return try_type_candidates[0];
}

auto spp::analyse::utils::type_utils::GetFwdTypes(
  asts::TypeAst const &type,
  scopes::ScopeManager const &sm)
  -> Pair<Shared<asts::TypeAst>, Shared<asts::TypeAst>> {
  //
  using asts::generate::common_types_precompiled::FWD_MUT;
  using asts::generate::common_types_precompiled::FWD_REF;

  // Generic types do not have forward types, so return nullptr.
  const auto type_sym = sm.CurrentScope->GetTypeSymbol(&type);
  if (type_sym->IsGeneric) { return {nullptr, nullptr}; }

  // Find the first FwdRef and first FwdMut super type in a single pass.
  auto fwd_ref_type = Shared<asts::TypeAst>(nullptr);
  auto fwd_mut_type = Shared<asts::TypeAst>(nullptr);
  const auto consider = [&](Shared<asts::TypeAst> const &candidate) {
    const auto bare = candidate->WithoutGenerics();
    if (fwd_ref_type == nullptr and TypeEq(*bare, *FWD_REF, *sm.CurrentScope, *sm.CurrentScope)) {
      fwd_ref_type = candidate;
    }
    else if (fwd_mut_type == nullptr and TypeEq(*bare, *FWD_MUT, *sm.CurrentScope, *sm.CurrentScope)) {
      fwd_mut_type = candidate;
    }
  };

  // Search the types for whichever marker is still missing.
  consider(type.WithoutGenerics());
  for (auto const &sup_type : type_sym->LinkedScope->SupTypes()) {
    if (fwd_ref_type != nullptr and fwd_mut_type != nullptr) { break; }
    consider(sup_type);
  }

  return {fwd_ref_type, fwd_mut_type};
}

auto spp::analyse::utils::type_utils::BuildFwdCall(
  asts::ExpressionAst const &receiver,
  asts::TypeAst const &receiver_type,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Unique<asts::PostfixExpressionAst> {
  // A type forwards by superimposing "FwdRef" or "FwdMut", whose coroutines are "fwd_ref" and "fwd_mut". The
  // immutable forward is preferred, matching how the forwarded-to members are resolved.
  const auto [fwd_ref_type, fwd_mut_type] = GetFwdTypes(receiver_type, *sm);
  if (fwd_ref_type == nullptr and fwd_mut_type == nullptr) { return nullptr; }

  // Build "<receiver>.fwd_ref()". The forwarding coroutines return a "GenOnce", so the call resumes itself and the
  // expression evaluates to the borrow of the forwarded-to value.
  auto field_name = MakeUnique<asts::IdentifierAst>(
    receiver.PosStart(), fwd_ref_type != nullptr ? "fwd_ref" : "fwd_mut");
  auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
  auto member_access = MakeUnique<asts::PostfixExpressionAst>(asts::AstClone(&receiver), std::move(field));
  auto func_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
  auto fwd_call = MakeUnique<asts::PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  // Analyse the built call, so that it can be inferred from and generated like any other analysed expression. The
  // receiver is analysed a second time here (it is a clone of an already analysed expression), which is what the
  // other operators that map themselves onto a method call do too.
  fwd_call->Stage7_AnalyseSemantics(sm, meta);
  return fwd_call;
}

auto spp::analyse::utils::type_utils::ValidateInconsistentTypes(
  Vec<asts::CaseExpressionBranchAst*> const &branches,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> Tup<Pair<asts::Ast*, Shared<asts::TypeAst>>, Vec<Pair<asts::Ast*, Shared<asts::TypeAst>>>> {
  //
  using errors::SppTypeMismatchError;
  using asts::generate::common_types_precompiled::NEVER;

  // Collect type information for each branch, pairing the
  // branch with its inferred type.
  auto branches_type_info = branches
    | genex::views::transform([&sm, meta](auto *x) { return MakePair(x, x->InferType(&sm, meta)); })
    | genex::to<Vec>();

  // The valued branches are branches that are non-terminating.
  // This is because "ret" from a branch doesn't pass a value
  // back to the binding, so shouldn't be considered for type
  // checking.
  auto valued_branches_type_info = branches_type_info
    | genex::views::remove_if([](auto const &x) { return x.first->Body->Terminates(); })
    | genex::to<Vec>();
  if (valued_branches_type_info.IsEmpty()) { valued_branches_type_info = branches_type_info; }

  // Filter the branch types down to variant types for custom
  // analysis.
  auto variant_branches_type_info = valued_branches_type_info
    | genex::views::filter([&sm](auto &&x) { return type_utils::IsTypeVariant(*x.second, *sm.CurrentScope); })
    | genex::to<Vec>();

  // Set the master branch type to the first branch's type, if
  // it exists. This is the default and may be subsequently
  // changed. Override it if an assignment type is given.
  auto master_branch_type_info = not valued_branches_type_info.IsEmpty()
    ? MakePair(valued_branches_type_info[0].first, valued_branches_type_info[0].second)
    : MakePair<asts::CaseExpressionBranchAst*, Shared<asts::TypeAst>>(nullptr, nullptr);
  if (meta->AssignmentTargetType != nullptr) {
    master_branch_type_info = MakePair(nullptr, meta->AssignmentTargetType);
  }

  // Otherwise, if there are variant branches, use the most
  // variant type as the master branch type.
  else if (not variant_branches_type_info.IsEmpty()) {
    auto most_inner_types = 0uz;
    for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
      const auto variant_size = DedupVariableInnerTypes(*variant_type, *sm.CurrentScope).Len();
      if (variant_size > most_inner_types) {
        master_branch_type_info = {variant_branch, variant_type};
        most_inner_types = variant_size;
      }
    }
  }

  // Remove the master branch pointer from the list of remaining
  // branch types and check all types match.
  // Todo: Shouldn't need to auto-remove "!" type, because TypeEq handles it?
  auto mismatch_branches_type_info = valued_branches_type_info
    | genex::views::remove_if([&](auto const &x) {
      return TypeEq(*NEVER, *x.second, *sm.CurrentScope, *sm.CurrentScope);
    })
    | genex::views::remove_if([&](auto const &x) {
      return x.first == master_branch_type_info.first;
    })
    | genex::views::remove_if([&](auto const &x) {
      return TypeEq(*master_branch_type_info.second, *x.second, *sm.CurrentScope, *sm.CurrentScope);
    })
    | genex::to<Vec>();

  if (not mismatch_branches_type_info.IsEmpty()) {
    const auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
    const auto [master_branch, master_branch_type] = master_branch_type_info;
    const auto final_member = master_branch ? master_branch->Body->FinalMember() : meta->AssignmentTarget.get();
    Raise<SppTypeMismatchError>(
      {sm.CurrentScope},
      ERR_ARGS(*final_member, *master_branch_type, *mismatch_branch->Body->FinalMember(), *mismatch_branch_type));
  }

  // The `master_branch_type_info.first` is deliberately null when an
  // assignment target type drove the master type (see above); calling
  // `To<>()` through that null pointer is UB, so guard it and keep
  // the null.
  const auto cast_master_branch_type_info = MakePair(
    master_branch_type_info.first ? master_branch_type_info.first->template ToUnchecked<asts::Ast>() : nullptr,
    master_branch_type_info.second);

  // Cast to common AST nodes and return with the types.
  const auto cast_branches_type_info = branches_type_info
    | genex::views::transform([](auto &&x) {
      return MakePair(x.first->template ToUnchecked<asts::Ast>(), x.second);
    })
    | genex::to<Vec>();
  return {cast_master_branch_type_info, cast_branches_type_info};
}

auto spp::analyse::utils::type_utils::GetAllAttrs(
  asts::TypeAst const &type,
  scopes::ScopeManager const &sm)
  -> Vec<Tup<Shared<asts::IdentifierAst>, scopes::TypeSymbol*, scopes::Scope*>> {
  // Get the symbol of the class type.
  const auto cls_sym = sm.CurrentScope->GetTypeSymbol(&type);

  // Get the attribute information from the class type and all super types.
  auto all_scopes = Vec{cls_sym->LinkedScope};
  all_scopes.AppendRange(cls_sym->LinkedScope->SupScopes());

  const auto all_syms = all_scopes
    | genex::views::filter([](auto &&sup_scope) {
      return sup_scope->AstNode->template To<asts::ClassPrototypeAst>() != nullptr;
    })
    | genex::views::transform([](auto &&sup_scope) {
      return MakePair(sup_scope, sup_scope->AllVarSymbols(true));
    })
    | genex::to<Vec>();

  auto extended_syms = Vec<Tup<Shared<asts::IdentifierAst>, scopes::TypeSymbol*, scopes::Scope*>>{};
  for (auto const &[sup_scope, syms] : all_syms) {
    for (auto const &sym : syms) {
      if (sym->IsGeneric) { continue; }
      const auto sym_type = sup_scope->GetTypeSymbol(sym->Type.get());
      extended_syms.PushBack({sym->Name, sym_type, sup_scope});
    }
  }

  return extended_syms;
}

static auto SupMemberAsMethod(
  spp::asts::Ast const *member)
  -> spp::asts::FunctionPrototypeAst const* {
  if (const auto fn = member->To<spp::asts::FunctionPrototypeAst>(); fn != nullptr) { return fn; }
  if (const auto ext = member->To<spp::asts::SupPrototypeExtensionAst>(); ext != nullptr and ext->Impl != nullptr) {
    const auto final_member = ext->Impl->FinalMember();
    return final_member != nullptr ? final_member->To<spp::asts::FunctionPrototypeAst>() : nullptr;
  }
  return nullptr;
}

namespace {
  auto _UnimplementedAbstractMethodsCache() -> spp::Map<
    spp::analyse::scopes::Scope const*,
    spp::Pair<std::uint64_t, spp::Vec<spp::asts::FunctionPrototypeAst const*>>>& {
    static auto cache = spp::Map<
      spp::analyse::scopes::Scope const*,
      spp::Pair<std::uint64_t, spp::Vec<spp::asts::FunctionPrototypeAst const*>>>();
    return cache;
  }
}

auto spp::analyse::utils::type_utils::ClearUnimplementedAbstractMethodsCache()
  -> void {
  _UnimplementedAbstractMethodsCache().clear();
}

auto spp::analyse::utils::type_utils::GetUnimplementedAbstractMethods(
  scopes::Scope const &type_scope)
  -> Vec<asts::FunctionPrototypeAst const*> {
  //
  using func_utils::SameSignature;

  // Every mention of a type asks this, and the answer is a property of the type rather than of the mention: it is read
  // off the methods of this scope and of the scopes above it, none of which change once the type is in place. The work
  // is a signature comparison of every abstract method against every concrete one, so recomputing it per mention is
  // what makes it one of the most expensive things in analysis.
  //
  // Keyed on the scope, and retired whenever the shape of the scope tree changes - which covers both a scope being
  // re-parented and a type gaining or losing the super scopes its inherited methods come from, since both bump the
  // linkage generation.
  auto &cache = _UnimplementedAbstractMethodsCache();
  const auto generation = scopes::TypeStructureGeneration();
  if (const auto hit = cache.find(&type_scope); hit != cache.end() and hit->second.first == generation) {
    return hit->second.second;
  }

  // Skip on functional types because of the awkward difference with the base method having "args: Args" tuple of
  // args, and implementations having their own individual args.
  // Todo: Autopack and check?
  const auto remember = [&](Vec<asts::FunctionPrototypeAst const*> answer) {
    cache[&type_scope] = {generation, answer};
    return answer;
  };

  if (type_scope.TySym != nullptr) {
    if (type_scope.TySym->Name->IsCompilerGeneratedType()) { return remember({}); }
    if (const auto fq_name = type_scope.TySym->FqName(); fq_name != nullptr and IsTypeFunc(*fq_name, type_scope)) {
      return remember({});
    }
  }

  // Gather every method visible on the type, from the type's own scope and from all of its super scopes, each tagged
  // with the scope that resolves the types in its signature.
  auto all_scopes = Vec<scopes::Scope const*>{&type_scope};
  all_scopes.AppendRange(type_scope.SupScopes());

  auto methods = Vec<Pair<scopes::Scope const*, asts::FunctionPrototypeAst const*>>();
  for (auto const *scope : all_scopes) {
    if (scope->AstNode == nullptr) { continue; }

    const auto impl = scope->AstNode->To<asts::ClassPrototypeAst>() == nullptr
      ? asts::AstBody(scope->AstNode)
      : Vec<asts::Ast*>{};
    if (impl.IsEmpty()) { continue; }

    for (const auto member : impl) {
      const auto fn = SupMemberAsMethod(member);
      if (fn == nullptr) { continue; }

      const auto method_scope = genex::find_if(
        scope->Children, [member](auto const &child) { return child->AstNode == member; });
      if (method_scope == scope->Children.end()) { continue; }

      methods.EmplaceBack(method_scope->get(), fn);
    }
  }

  // A method stays abstract only while nothing anywhere on the type implements its signature. "SameSignature" answers
  // false for any two methods whose names differ, so only the concrete methods sharing a name with the abstract one can
  // implement it; indexing them by name turns a scan of every method on the type into a scan of its overloads, which
  // for a type with many single-overload methods is the difference between a square and a line.
  auto concrete_by_name = Map<
    spp::utils::InternedId, Vec<Pair<scopes::Scope const*, asts::FunctionPrototypeAst const*>>>();
  for (auto const &entry : methods) {
    if (entry.second->AbstractAnnotation != nullptr) { continue; }
    concrete_by_name[entry.second->Name->NameId()].EmplaceBack(entry);
  }

  auto unimplemented = Vec<asts::FunctionPrototypeAst const*>();
  for (auto const &[abs_scope, abs_fn] : methods) {
    if (abs_fn->AbstractAnnotation == nullptr) { continue; }

    const auto candidates = concrete_by_name.find(abs_fn->Name->NameId());
    const auto is_implemented = candidates != concrete_by_name.end()
      and genex::any_of(candidates->second, [&](auto const &other) {
        return SameSignature(*other.second, *other.first, *abs_fn, *abs_scope);
      });

    if (not is_implemented) { unimplemented.EmplaceBack(abs_fn); }
  }

  return remember(std::move(unimplemented));
}

auto spp::analyse::utils::type_utils::GetAllAttrAsts(
  asts::TypeAst const &type,
  scopes::ScopeManager const &sm)
  -> Vec<asts::ClassAttributeAst*> {
  // Get the symbol of the class type.
  const auto cls_sym = sm.CurrentScope->GetTypeSymbol(&type);

  // Walk the class and its super types in the same order as "GetAllAttrs", so the results line up index for index.
  auto all_scopes = Vec{cls_sym->LinkedScope};
  all_scopes.AppendRange(cls_sym->LinkedScope->SupScopes());

  auto attr_asts = Vec<asts::ClassAttributeAst*>{};
  for (auto const &sup_scope : all_scopes) {
    const auto cls_proto = sup_scope->AstNode->To<asts::ClassPrototypeAst>();
    if (cls_proto == nullptr) { continue; }
    for (auto const &member : cls_proto->Impl->Members) {
      if (const auto attr = member->To<asts::ClassAttributeAst>(); attr != nullptr) {
        attr_asts.EmplaceBack(attr);
      }
    }
  }

  return attr_asts;
}

auto spp::analyse::utils::type_utils::GetTypeSymOrError(
  scopes::Scope const &scope,
  asts::TypeIdentifierAst const &type_part,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData *)
  -> scopes::TypeSymbol* {
  //
  using expr_utils::RaiseMissingTypeIdentifierAndClosestOptions;

  // Get the type part's symbol, and raise an error if it doesn't exist.
  const auto type_sym = scope.GetTypeSymbol(&type_part, false);
  if (type_sym == nullptr) {
    RaiseMissingTypeIdentifierAndClosestOptions(type_part, scope.AllTypeSymbols(), sm);
  }

  // Return the found type symbol.
  return type_sym;
}

auto spp::analyse::utils::type_utils::GetNsScopeOrError(
  scopes::Scope const &scope,
  asts::IdentifierAst const &ns,
  scopes::ScopeManager const &sm)
  -> scopes::Scope* {
  //
  using expr_utils::RaiseMissingIdentifierAndClosestOptions;

  // If the namespace does not exist, raise an error.
  const auto ns_sym = scope.GetNsSymbol(&ns);
  if (ns_sym == nullptr) {
    RaiseMissingIdentifierAndClosestOptions(ns, {}, scope.AllNsSymbols(), sm);
  }

  // Return the found namespace scope.
  return ns_sym->LinkedScope;
}

auto spp::analyse::utils::type_utils::EnforceGenericConstraintsOneArg(
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

auto spp::analyse::utils::type_utils::DedupVariableInnerTypes(
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

auto spp::analyse::utils::type_utils::RecursiveAliasSearch(
  asts::TypeStatementAst const &alias_stmt,
  const bool from_use_stmt,
  scopes::Scope *tracking_scope,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Tup<Shared<asts::TypeAst>, Shared<asts::GenericParameterGroupAst>, scopes::Scope*> {
  //
  using generic_bindings::NameGnArgs;

  // How to extract generic parameters from a type symbol: alias, then type, otherwise none (generic).
  const auto NO_PARAMS = asts::GenericParameterGroupAst::NewEmpty();
  const auto extract_params = [&NO_PARAMS](scopes::TypeSymbol const &ts) {
    return ts.Alias
      ? ts.Alias->Params.get()
      : ts.Type
      ? ts.Type->GnParamGroup.get()
      : NO_PARAMS.get();
  };

  const auto filter_params = [](asts::GenericParameterGroupAst const &pg, asts::GenericArgumentGroupAst const &ag) {
    auto out = asts::GenericParameterGroupAst::NewEmptyShared();
    for (auto const &param : pg.GetTypeParams()) {
      if (not genex::any_of(ag.GetTypeKeywordArgs(), [&](auto const *arg) { return *arg->Name == *param->Name; })) {
        out->Params.EmplaceBack(asts::AstClone(param));
      }
    }
    for (auto const &param : pg.GetCompParams()) {
      if (not genex::any_of(ag.GetCompKeywordArgs(), [&](auto const *arg) { return *arg->Name == *param->Name; })) {
        out->Params.EmplaceBack(asts::AstClone(param));
      }
    }
    return out;
  };

  // Get the next type in the search, and its symbol.
  auto old_type = alias_stmt.OldType;
  auto old_sym = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics().get());

  // If this is a use statement to a class, then grab its generics and return immediately.
  // For example, use Vec::Vec => type Vec[T, A: ... = ...] = Vec::Vec[T=T, A=A]
  if (from_use_stmt and old_sym->Alias == nullptr) {
    auto generic_params = old_sym->Type->GnParamGroup;
    old_type = old_type->WithGenerics(asts::GenericArgumentGroupAst::FromParams(*generic_params));
    return {old_type, generic_params, old_sym->LinkedScope};
  }

  const auto generic_args = asts::AstClone(old_type->LastTypePart()->GnArgGroup.get());
  auto final_generic_params = asts::GenericParameterGroupAst::NewEmptyShared();
  tracking_scope = old_sym->ScopeDefinedIn;

  // The walk below has no base case beyond "the next name is not an alias", so an alias that leads back to one
  // already being followed is followed forever. Statements are what is recorded rather than symbols, because a
  // statement is what the source wrote and so is what the error can point at; the starting one is seeded so that a
  // self-alias ("type A = A") is caught on its first step rather than on its second.
  auto followed_aliases = Vec{&alias_stmt};

  while (true) {
    // A "use" alias declares no parameters of its own - it renames a type without reshaping it - so arguments
    // written at the use site belong to whatever it names rather than being bound here. Every other alias binds
    // them to the parameters it declares, which is what naming and substituting them does.
    const auto passes_generics_through = old_sym->Alias != nullptr and old_sym->Alias->FromUseStmt;

    if (not passes_generics_through) {
      const auto is_tuple = IsTupSymbol(*old_sym);
      NameGnArgs(*old_type->LastTypePart()->GnArgGroup, *extract_params(*old_sym), *old_type, *sm, *meta, is_tuple);
      if (old_sym->Alias) {
        final_generic_params = filter_params(*old_sym->Alias->Params, *old_type->LastTypePart()->GnArgGroup);
      }
      old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs());
      if (not is_tuple) { *generic_args += *old_type->LastTypePart()->GnArgGroup; }
    }
    tracking_scope = old_sym->ScopeDefinedIn;

    if (old_sym->Alias == nullptr) { break; }
    RaiseIf<errors::SppTypeAliasCyclicError>(
      genex::contains(followed_aliases, old_sym->Alias->Stmt),
      {sm->CurrentScope}, ERR_ARGS(alias_stmt, *old_sym->Alias->Stmt));
    followed_aliases.EmplaceBack(old_sym->Alias->Stmt);

    // Follow the alias, handing the arguments straight on when it is one that passes them through.
    auto const *carried = old_type->LastTypePart()->GnArgGroup.get();
    const auto carries_generics = passes_generics_through and not carried->Args.IsEmpty();
    old_type = carries_generics
      ? old_sym->Alias->Written->WithGenerics(asts::AstClone(carried))
      : old_sym->Alias->Written;
    old_sym = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics().get());

    // Arguments just handed on still have to be bound by whatever received them, so the walk goes round once more
    // even when that is a class rather than another alias.
    if (old_sym->Alias == nullptr and not carries_generics) { break; }
  }

  old_type = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics().get())->FqName()->WithGenerics(
    AstClone(old_type->LastTypePart()->GnArgGroup));

  auto &temp = *old_type->LastTypePart()->GnArgGroup;
  NameGnArgs(
    temp, *extract_params(*old_sym), *old_type, *sm, *meta, IsTupSymbol(*old_sym));
  old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs());
  return {old_type, final_generic_params, tracking_scope};
}

auto spp::analyse::utils::type_utils::GetFieldIndexInType(
  asts::TypeAst const &type_sym,
  asts::IdentifierAst const &field_name,
  scopes::ScopeManager const &sm)
  -> std::size_t {
  // A class superimposing "Gen"/"GenOnce"/a "FunXXX" gets that interface's fat-pointer fields prepended ahead of
  // its own declared attributes (see "ClassPrototypeAst::FillLlvmLayout"), so an attribute's declared index has
  // to be shifted past them.
  const auto base = GetSuperimposedFatPointerFieldCount(type_sym, *sm.CurrentScope);

  // Get all the attributes on the type.
  const auto all_attrs = GetAllAttrs(type_sym, sm);

  // Find the field index.
  for (auto index = 0uz; index < all_attrs.Len(); ++index) {
    if (*spp::get<0>(all_attrs[index]) == field_name) {
      return base + index;
    }
  }

  return base + all_attrs.Len();
}

auto spp::analyse::utils::type_utils::ResolveAndSubstituteSelfType(
  asts::TypeAst const &type,
  scopes::Scope const &scope,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData &meta)
  -> Shared<asts::TypeAst> {
  // Todo: always clone here? performance hit i think.
  using asts::generate::common_types::SelfType;
  const auto true_self_type = scope.GetEnclosingSelfType(meta);
  if (true_self_type == nullptr) { return AstClone(&type); }

  // If "Self" is not present, return a plain clone.
  if (genex::none_of(type.Iterator(), [](auto const &part) { return part->Name == "Self"; })) {
    return AstClone(&type);
  }

  // Substitute "Self" with the concrete enclosing type.
  const auto g = MakeUnique<asts::GenericArgumentTypeKeywordAst>(SelfType(0), nullptr, true_self_type);
  const auto args = Vec<asts::GenericArgumentAst*>{g.get()};

  auto t = type.SubstituteGenerics(args);
  meta.Save();
  meta.AllowAbstractType = true;
  t->Stage7_AnalyseSemantics(&sm, &meta);
  meta.Restore();
  return t;
}
