module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_predicates;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.type_members;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ptr;
import genex;
import std;

/// [CHECKED]
auto spp::analyse::utils::type_predicates::AnyTypePart(
  TypeAst const &type,
  FuncRef<bool(TypeIdentifierAst const &)> part,
  Func<bool(IdentifierAst const &)> const &comp_name)
  -> bool {
  // A convention or a nested name is walked through to the name it is on: "&T" is "T"; "A::B" is its left-hand side.
  if (auto const *const unary = type.To<TypeUnaryExpressionAst>(); unary != nullptr) {
    return AnyTypePart(*unary->Rhs, part, comp_name);
  }
  if (auto const *const postfix = type.To<TypePostfixExpressionAst>(); postfix != nullptr) {
    return AnyTypePart(*postfix->Lhs, part, comp_name);
  }
  auto const *const name = type.To<TypeIdentifierAst>();
  if (name == nullptr) { return false; }
  if (part(*name)) { return true; }
  for (auto const &arg : name->GnArgGroup->Args) {
    if (arg->IsCompArg()) {
      if (AnyCompPart(*arg->CompVal, comp_name, [&](TypeAst const &owner) {
        return AnyTypePart(owner, part, comp_name);
      })) { return true; }
    }
    else if (arg->IsTypeArg() and AnyTypePart(*arg->TypeVal, part, comp_name)) { return true; }
  }
  return false;
}

auto spp::analyse::utils::type_predicates::AnyCompPart(
  ExpressionAst const &expr,
  Func<bool(IdentifierAst const &)> const &name,
  Func<bool(TypeAst const &)> const &owner,
  Func<bool(ExpressionAst const &)> const &value) -> bool {
  // A pack is its elements, each asked on its own.
  if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) {
    return genex::any_of(tup->Elems, [&](auto const &elem) { return AnyCompPart(*elem, name, owner, value); });
  }
  if (value != nullptr and value(expr)) { return true; }
  if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    return AnyCompPart(*paren->Expr, name, owner, value);
  }
  if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr) {
    const auto [lhs, rhs] = bin->Operands();
    return (lhs != nullptr and AnyCompPart(*lhs, name, owner, value))
      or (rhs != nullptr and AnyCompPart(*rhs, name, owner, value));
  }
  if (owner != nullptr and asts::IsStaticMemberAccess(&expr)) {
    auto const *const type = expr.To<PostfixExpressionAst>()->Lhs->To<TypeAst>();
    return type != nullptr and owner(*type);
  }
  const auto id = expr.To<IdentifierAst>();
  return id != nullptr and name != nullptr and name(*id);
}

auto spp::analyse::utils::type_predicates::DoesTypeNameSelf(
  TypeAst const &type) -> bool {
  // Any part, at any depth ("Opt[Self]", "&Self").
  return AnyTypePart(type, [](TypeIdentifierAst const &part) { return part.IsSelfType(); });
}

auto spp::analyse::utils::type_predicates::DoesTypeNameAGnParam(
  TypeAst const &type, GenericParameterAst const &param,
  Scope const &scope) -> bool {
  // The parameter's identity, given when declared ("GenericParameterAst::Stage2_GenTopLvlScopes"); each part is the
  // parameter (or a binding of it) it resolves to where read. Walked as written, not read off the type's identity: an
  // alias over a template not made yet ("SizedIntegerUnsigned[w]") is keyed with its arguments spelled, so its identity
  // names no parameter that the written arguments do.
  const auto type_param = param.IsTypeParam() ? param.ParamId() : 0;
  const auto comp_param = param.IsCompParam() ? param.ParamId() : 0;
  return AnyTypePart(
    type,
    [&](TypeIdentifierAst const &part) {
      auto const *const sym = type_param != 0 ? scope.FindTypeSymbol(&part) : nullptr;
      return sym != nullptr and sym->ParamId() == type_param;
    },
    [&](IdentifierAst const &name) {
      auto const *const sym = comp_param != 0 ? scope.FindVarSymbol(&name) : nullptr;
      return sym != nullptr and sym->ParamId() == comp_param;
    });
}

auto spp::analyse::utils::type_predicates::IsOrExtendsTemplate(
  TypeSymbol const &sym, Scope const &scope,
  TypeSymbol const *tmpl) -> bool {
  // Check for a template match; is the template of symbol
  // equal to "tmpl"?
  if (TypeRef::ForKindCheck(sym, scope).Template() == tmpl) { return true; }
  return sym.LinkedScope != nullptr and genex::any_of(
    type_members::SuperClsScopes(sym.LinkedScope->GetSupScopes()),
    [tmpl](auto const *cls_scope) { return TypeRef::ForKindCheck(*cls_scope).Template() == tmpl; });
}

auto spp::analyse::utils::type_predicates::IsTypeGenerator(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Either the Gen or GenOnce generator type, discarding generics.
  using generate::common_types_precompiled::GEN;
  using generate::common_types_precompiled::GEN_ONCE;
  return ref.KindSymbol() != nullptr and (ref.IsA(*GEN, scope) or ref.IsA(*GEN_ONCE, scope));
}

auto spp::analyse::utils::type_predicates::IsTypeTuple(
  TypeRef const &ref, Scope const &scope) -> bool {
  using generate::common_types_precompiled::TUP;
  return ref.KindSymbol() != nullptr and ref.IsA(*TUP, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeArray(
  TypeRef const &ref, Scope const &scope) -> bool {
  using generate::common_types_precompiled::ARR;
  return ref.KindSymbol() != nullptr and ref.IsA(*ARR, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVariant(
  TypeRef const &ref, Scope const &scope) -> bool {
  // A borrowed variant is still one.
  using generate::common_types_precompiled::VAR;
  return not ref.IsNever and ref.Symbol != nullptr and ref.IsA(*VAR, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeFunction(
  TypeRef const &ref, Scope const &scope) -> bool {
  // Either the FunMov, FunMut or FunRef function type, discarding generics, or a "$" mock of one.
  using generate::common_types_precompiled::FUN_MOV;
  using generate::common_types_precompiled::FUN_MUT;
  using generate::common_types_precompiled::FUN_REF;
  const auto sym = ref.KindSymbol();
  return sym != nullptr and (
    sym->IsMock() or ref.IsA(*FUN_MOV, scope) or ref.IsA(*FUN_MUT, scope) or ref.IsA(*FUN_REF, scope));
}

auto spp::analyse::utils::type_predicates::IsTypeCompTimeIndexable(
  TypeRef const &ref, Scope const &scope) -> bool {
  return IsTypeTuple(ref, scope) or IsTypeArray(ref, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeBool(
  TypeRef const &ref, Scope const &scope) -> bool {
  using generate::common_types_precompiled::BOOL;
  return ref.KindSymbol() != nullptr and ref.IsA(*BOOL, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVoid(
  TypeRef const &ref, Scope const &scope) -> bool {
  using generate::common_types_precompiled::VOID;
  return ref.KindSymbol() != nullptr and ref.IsA(*VOID, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTry(
  TypeRef const &ref, Scope const &scope) -> bool {
  using generate::common_types_precompiled::TRY;
  return ref.KindSymbol() != nullptr and ref.IsA(*TRY, scope);
}

namespace {
  /// A generic template named with no arguments: it stands for itself over its own parameters, which nothing has bound
  /// - unless every parameter is a pack, which no arguments bind to the empty pack: "Tup" with none is the empty tuple,
  /// which is how "()" is named.
  auto IsUnboundTemplate(
    spp::analyse::scopes::TypeSymbol const &sym) -> bool {
    return sym.IsBareTemplate() and not genex::all_of(
      sym.Type->GnParamGroup->Params, [](auto const &param) { return param->IsVariadic(); });
  }
}

auto spp::analyse::utils::type_predicates::IsTypeSignedInteger(
  TypeAst const &type,
  Scope const &scope) -> bool {
  auto const *const sym = scope.FindTypeSymbol(&type);
  const auto signed_id = sym != nullptr ? sym->CompArgId("signed") : nullptr;
  auto const *const is_signed = signed_id != nullptr ? signed_id->AsBool() : nullptr;
  return is_signed != nullptr and *is_signed;
}

auto spp::analyse::utils::type_predicates::IsTypeConcrete(
  TypeRef const &ref) -> bool {
  // A template named bare, its parameters unbound ("IsUnboundTemplate").
  if (ref.Symbol != nullptr and IsUnboundTemplate(*ref.Symbol)) { return false; }
  if (ref.Id == nullptr) { return ref.Symbol != nullptr; }
  return scopes::IsConcreteTypeId(ref.Id);
}

auto spp::analyse::utils::type_predicates::IsTypeConcrete(
  TypeAst const &type, Scope const &scope) -> bool {
  // By what it names here ("Scope::PartialTypeIdOf"). A name that resolves to nothing is deliberately not treated as a
  // parameter: a nested argument is looked up in the instantiation's own scope, which need not have every type its
  // arguments were written in terms of in view, and reading "not found" as "still generic" would refuse perfectly good
  // instantiations.
  // A template named with no arguments, its parameters unbound ("IsUnboundTemplate").
  auto const *const head = scope.FindHeadSymbol(type);
  if (head != nullptr and type.LastTypePart()->GnArgGroup->Args.empty() and IsUnboundTemplate(*head)) { return false; }
  return scopes::IsConcreteTypeId(scope.PartialTypeIdOf(type));
}

/// [CHECKED]
auto spp::analyse::utils::type_predicates::IsTypeBorrowed(
  TypeAst const &type, ScopeManager const &sm, const bool deep) -> bool {
  // Check that either this type, or any inner types for variants,
  // are "&" or "&mut". Start with short-circuits on the type given,
  // which might contain an "&"/"&mut" unary operator.
  if (type.GetConvention() != nullptr) { return true; }
  if (type.IsSelfType()) { return false; }

  // A generic bound to a borrow is one ("T" with "T=&mut Str").
  const auto ref = TypeRef::Of(type, *sm.CurrentScope);
  if (ref.IsBorrowed()) { return true; }

  // A variant is borrowed when any member is: the members are
  // flattened through nested variants already.
  if (deep and IsTypeVariant(ref, *sm.CurrentScope)) {
    for (auto const &member : type_members::VariantMemberRefs(ref, *sm.CurrentScope)) {
      if (member.IsBorrowed()) { return true; }
    }
  }

  // No borrowing of any nature discovered => non borrowable type.
  // Checked all depths.
  return false;
}
