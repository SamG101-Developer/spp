module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_predicates;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.type_compare;
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
  std::function<bool(TypeIdentifierAst const &)> const &part,
  std::function<bool(IdentifierAst const &)> const &comp_name)
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
  std::function<bool(IdentifierAst const &)> const &name,
  std::function<bool(TypeAst const &)> const &owner,
  std::function<bool(ExpressionAst const &)> const &value) -> bool {
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

auto spp::analyse::utils::type_predicates::IsTypeConcrete(
  TypeRef const &ref) -> bool {
  // A generic template named bare stands for itself over its own parameters, which nothing has bound.
  if (ref.Symbol != nullptr and ref.Symbol->IsBareTemplate()) { return false; }
  if (ref.Id == nullptr) { return ref.Symbol != nullptr; }
  return scopes::IsConcreteTypeId(ref.Id);
}

auto spp::analyse::utils::type_predicates::IsTypeConcrete(
  TypeAst const &type, Scope const &scope) -> bool {
  // By what it names here ("Scope::PartialTypeIdOf"). A name that resolves to nothing is deliberately not treated as a
  // parameter: a nested argument is looked up in the instantiation's own scope, which need not have every type its
  // arguments were written in terms of in view, and reading "not found" as "still generic" would refuse perfectly good
  // instantiations.
  // A template named with no arguments stands for itself over its own parameters - unless every parameter is a pack,
  // which no arguments bind to the empty pack: "Tup" with none is the empty tuple, which is how "()" is named.
  auto const *const head = scope.FindHeadSymbol(type);
  if (head != nullptr and head->IsBareTemplate() and type.LastTypePart()->GnArgGroup->Args.IsEmpty()) {
    const auto all_packs = genex::all_of(head->Type->GnParamGroup->Params, [](auto const &param) {
      return param->IsVariadic();
    });
    if (not all_packs) { return false; }
  }
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
    for (auto const &member : type_compare::VariantMemberRefs(ref, *sm.CurrentScope)) {
      if (member.IsBorrowed()) { return true; }
    }
  }

  // No borrowing of any nature discovered => non borrowable type.
  // Checked all depths.
  return false;
}
