module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_predicates;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.type_members;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_index_ast;
import spp.asts.spread_expression_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts, ArrayLiteralExplicitElementsAst);
use_alias(spp::asts, ArrayLiteralRepeatedElementAst);
use_alias(spp::asts, BinaryExpressionAst);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, GenericArgumentAst);
use_alias(spp::asts, GenericParameterAst);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, ObjectInitializerAst);
use_alias(spp::asts, ParenthesisedExpressionAst);
use_alias(spp::asts, PostfixExpressionAst);
use_alias(spp::asts, PostfixExpressionOperatorFunctionCallAst);
use_alias(spp::asts, PostfixExpressionOperatorIndexAst);
use_alias(spp::asts, SpreadExpressionAst);
use_alias(spp::asts, TupleLiteralAst);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeIdentifierAst);
use_alias(spp::asts, TypePostfixExpressionAst);
use_alias(spp::asts, TypeUnaryExpressionAst);

namespace spp::analyse::utils::type_predicates {
  /// A generic template named with no arguments: it stands for itself over its
  /// own parameters, which nothing has bound - unless every parameter is a pack
  /// which no arguments bind to the empty pack, like "Tup". "Vec" is an unbound
  /// template because it needs "T". Todo: Consider optionals?
  static auto _IsUnboundTemplate(TypeSymbol const &sym) -> bool;
}

auto spp::analyse::utils::type_predicates::_IsUnboundTemplate(
  TypeSymbol const &sym) -> bool {
  // Check for a bare template, where the args are not variadics, ie needed.
  // Todo: again, do we need to consider optionals? As they will provide args?
  const auto variadic = [](auto const &param) { return param->IsVariadic(); };
  return sym.IsBareTemplate() and not genex::all_of(
    sym.Type->GnParamGroup->Params, variadic);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::AnyTypePart(
  TypeAst const &type, FuncRef<bool(TypeIdentifierAst const &)> type_match,
  Func<bool(IdentifierAst const &)> const &comp_match) -> bool {
  // For a unary type expression, move into the rhs part. This moves past
  // namespaces and borrow operations.
  const auto unary = type.To<TypeUnaryExpressionAst>();
  if (unary != nullptr) {
    return AnyTypePart(*unary->Rhs, type_match, comp_match);
  }

  // For a postfix type expression, move into the lhs part. This moves backwards
  // from superimposition types.
  const auto postfix = type.To<TypePostfixExpressionAst>();
  if (postfix != nullptr) {
    return AnyTypePart(*postfix->Lhs, type_match, comp_match);
  }

  // Otherwise, this is a type identifier ast, so cast it and run the standard
  // predicate on it. Return for a match.
  const auto name = type.To<TypeIdentifierAst>();
  if (name == nullptr) { return false; }
  if (type_match(*name)) { return true; }

  // Check the generic arguments next. For "cmp" arguments, find a comp arg
  // whose name matches the filter should it exist, and check its
  for (auto const &arg : name->GnArgGroup->Args) {
    if (arg->IsCompArg()) {
      if (AnyCompPart(*arg->CompVal, comp_match, [&](TypeAst const &owner) { return AnyTypePart(owner, type_match, comp_match); })) {
        return true;
      }
    }
    else if (arg->IsTypeArg() and AnyTypePart(*arg->TypeVal, type_match, comp_match)) { return true; }
  }
  return false;
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::AnyCompPart(
  ExpressionAst const &expr,
  Func<bool(IdentifierAst const &)> const &comp_match,
  Func<bool(TypeAst const &)> const &owner,
  Func<bool(ExpressionAst const &)> const &value) -> bool {
  // If a value filter is provided, check for the value to match. If it does
  // match, then return early. Every value is asked before its parts are.
  if (value != nullptr and value(expr)) { return true; }

  // Move inside a parenthesised expression for checking.
  if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    return AnyCompPart(*paren->Expr, comp_match, owner, value);
  }

  // Move inside a spread expression for checking.
  if (const auto spread = expr.To<SpreadExpressionAst>(); spread != nullptr) {
    return AnyCompPart(*spread->Expr, comp_match, owner, value);
  }

  // Move inside the left and right operands for the binary expression checking.
  if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr) {
    const auto [lhs, rhs] = bin->Operands();
    return (lhs != nullptr and AnyCompPart(*lhs, comp_match, owner, value))
      or (rhs != nullptr and AnyCompPart(*rhs, comp_match, owner, value));
  }

  // Check the inner parts of tuples, arrays, and object initialisers.
  const auto any = [&](ExpressionAst const *part) {
    return part != nullptr and AnyCompPart(*part, comp_match, owner, value);
  };
  const auto any_elem = [&](auto const &elems) {
    return genex::any_of(elems, [&](auto const &elem) { return any(elem.get()); });
  };
  const auto any_arg = [&](auto const &args) {
    return genex::any_of(args, [&](auto const &arg) { return any(arg->Val.get()); });
  };

  if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) { return any_elem(tup->Elems); }
  if (const auto arr = expr.To<ArrayLiteralExplicitElementsAst>(); arr != nullptr) { return any_elem(arr->Elems); }
  if (const auto rep = expr.To<ArrayLiteralRepeatedElementAst>(); rep != nullptr) {
    return any(rep->Elem.get()) or any(rep->Size.get());
  }
  if (const auto obj = expr.To<ObjectInitializerAst>(); obj != nullptr) {
    return (owner != nullptr and obj->Type != nullptr and owner(*obj->Type)) or any_arg(obj->ArgGroup->Args);
  }

  // Postfix checks
  if (const auto postfix = expr.To<PostfixExpressionAst>(); postfix != nullptr) {
    if (asts::IsStaticMemberAccess(&expr)) {
      const auto type = postfix->Lhs->To<TypeAst>();
      return owner != nullptr and type != nullptr and owner(*type);
    }

    // Check the left-hand-side of the postfix expression for matches, by moving
    // inwards.
    if (any(postfix->Lhs.get())) { return true; }

    // For function calls, check the function call arguments.
    if (const auto call = postfix->Op->To<PostfixExpressionOperatorFunctionCallAst>(); call != nullptr) {
      return any_arg(call->FnArgGroup->Args);
    }

    // For an index expression, check the element being accessed. Todo: This
    // might be dead/redundant code. Also slicing?
    const auto index = postfix->Op->To<PostfixExpressionOperatorIndexAst>();
    return index != nullptr and any(index->Expr.get());
  }

  // Finally, do an id check, and check it against the "name" function for a
  // match.
  const auto id = expr.To<IdentifierAst>();
  return id != nullptr and comp_match != nullptr and comp_match(*id);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::DoesTypeNameSelf(
  TypeAst const &type) -> bool {
  // Map into the part walker, checking each part for a "Self" usage. This gets
  // it from any depth, such as behind a borrow or a nested generic argument.
  return AnyTypePart(
    type, [](TypeIdentifierAst const &part) { return part.IsSelfType(); });
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::DoesTypeNameAGnParam(
  TypeAst const &type, GenericParameterAst const &param,
  Scope const &scope) -> bool {
  // Get the type or comp param as id. They are two separate values, allowing
  // for proper searching below. Always invokes into a type search first.
  const auto type_param = param.IsTypeParam() ? param.ParamId() : 0;
  const auto comp_param = param.IsCompParam() ? param.ParamId() : 0;

  // Enter into the type part searcher (as this starts with a type), and then
  // moves into inner type and comp generics.
  return AnyTypePart(
    type,
    [&](TypeIdentifierAst const &part) {
      const auto sym = type_param != 0 ? scope.FindTypeSymbol(&part) : nullptr;
      return sym != nullptr and sym->ParamId() == type_param;
    },
    [&](IdentifierAst const &name) {
      const auto sym = comp_param != 0 ? scope.FindVarSymbol(&name) : nullptr;
      return sym != nullptr and sym->ParamId() == comp_param;
    });
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsOrExtendsTemplate(
  TypeSymbol const &sym, Scope const &scope, TypeSymbol const *tmpl) -> bool {
  // Check for a template match; is the template of symbol equal to "tmpl"? This
  // also converts it being the template itself.
  if (TypeRef::ForKindCheck(sym, scope).Template() == tmpl) { return true; }

  // Otherwise, search the super scopes and apply the same check, ie is the type
  // ref for that class scope the template match.
  return sym.LinkedScope != nullptr and genex::any_of(
    type_members::SuperClsScopes(sym.LinkedScope->GetSupScopes()),
    [tmpl](auto const *cls_scope) { return TypeRef::ForKindCheck(*cls_scope).Template() == tmpl; });
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeGenerator(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Gen" and "GenOnce" generator types, discarding generics.
  using asts::generate::common_types_precompiled::GEN;
  using asts::generate::common_types_precompiled::GEN_ONCE;
  return ref.KindSymbol() != nullptr and (
    ref.IsA(*GEN, scope) or ref.IsA(*GEN_ONCE, scope));
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeTuple(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Tup" tuple type, discarding generics.
  using asts::generate::common_types_precompiled::TUP;
  return ref.KindSymbol() != nullptr and ref.IsA(*TUP, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeArray(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Arr" array type, discarding generics.
  using asts::generate::common_types_precompiled::ARR;
  return ref.KindSymbol() != nullptr and ref.IsA(*ARR, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypePointer(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "NonNull" and "Ptr" pointer types, discarding generics.
  using asts::generate::common_types_precompiled::NON_NULL;
  using asts::generate::common_types_precompiled::PTR;
  return ref.KindSymbol() != nullptr and (
    ref.IsA(*NON_NULL, scope) or ref.IsA(*PTR, scope));
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeVariant(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Var" variant type, discarding generics.
  using asts::generate::common_types_precompiled::VAR;
  return not ref.IsNever and ref.Symbol != nullptr and ref.IsA(*VAR, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeFunction(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "FunRef", "FunMut" and "FunMov" function types,
  // discarding generics. The "$" types are allowed to pass here.
  using asts::generate::common_types_precompiled::FUN_MOV;
  using asts::generate::common_types_precompiled::FUN_MUT;
  using asts::generate::common_types_precompiled::FUN_REF;
  const auto sym = ref.KindSymbol();
  return sym != nullptr and (
    sym->IsMock()
    or ref.IsA(*FUN_MOV, scope)
    or ref.IsA(*FUN_MUT, scope)
    or ref.IsA(*FUN_REF, scope));
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeCompTimeIndexable(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Tup" tuple and "Arr" array types, discarding generics.
  return IsTypeTuple(ref, scope) or IsTypeArray(ref, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeBool(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Bool" boolean type.
  using asts::generate::common_types_precompiled::BOOL;
  return ref.KindSymbol() != nullptr and ref.IsA(*BOOL, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeVoid(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Void" void type.
  using asts::generate::common_types_precompiled::VOID;
  return ref.KindSymbol() != nullptr and ref.IsA(*VOID, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeTry(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Check against the "Try" try type.
  using asts::generate::common_types_precompiled::TRY;
  return ref.KindSymbol() != nullptr and ref.IsA(*TRY, scope);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeSignedInteger(
  TypeAst const &type, Scope const &scope) -> bool {
  // Check against the signed integer types, by getting the full integer type
  // and checking the "signed" generic param.
  const auto sym = scope.FindTypeSymbol(&type);
  const auto signed_id = sym != nullptr ? sym->CompArgId("signed") : nullptr;
  const auto is_signed = signed_id != nullptr ? signed_id->AsBool() : nullptr;
  return is_signed != nullptr and *is_signed;
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeConcrete(
  TypeAst const &type, Scope const &scope) -> bool {
  // Guaranteed non-concrete if it is an unbound template, so short-circuit if
  // that's the case. Otherwise, use the interned version.
  const auto head = scope.FindHeadSymbol(type);
  if (head != nullptr and type.LastTypePart()->GnArgGroup->Args.empty() and _IsUnboundTemplate(*head)) { return false; }
  return scopes::IsConcreteTypeId(scope.PartialTypeIdOf(type));
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeConcrete(
  TypeRef const &ref) -> bool {
  // Similar to above, but as the TypeRef carries the ID, we can just call the
  // interned version directly.
  if (ref.Symbol != nullptr and _IsUnboundTemplate(*ref.Symbol)) { return false; }
  if (ref.Id == nullptr) { return ref.Symbol != nullptr; }
  return scopes::IsConcreteTypeId(ref.Id);
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeBorrowed(
  TypeAst const &type, Scope const &scope, const bool deep) -> bool {
  // Check that either this type, or any inner types for variants, are "&" or
  // "&mut". Start with short-circuits on the type given, which might contain an
  // "&"/"&mut" unary operator.
  if (type.GetConvention() != nullptr) { return true; }
  if (type.IsSelfType()) { return false; }

  // A generic bound to a borrow is one ("T" with "T=&mut Str").
  const auto ref = TypeRef::Of(type, scope);
  if (ref.IsBorrowed()) { return true; }

  // A variant is borrowed when any member is: the members are flattened through
  // nested variants already. Todo: Is this correct? "One-for-all"?
  if (deep and IsTypeVariant(ref, scope)) {
    for (auto const &member : type_members::VariantMemberRefs(ref, scope)) {
      if (member.IsBorrowed()) { return true; }
    }
  }

  // No borrowing of any nature discovered => non borrowable type. Checked all
  // depths.
  return false;
}
