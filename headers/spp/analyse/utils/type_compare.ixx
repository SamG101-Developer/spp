module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_compare;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

namespace spp::analyse::utils::type_compare {
  SPP_EXP_CLS using GenericInferenceMap = Map<
    Shared<TypeIdentifierAst>, ExpressionAst*,
    spp::utils::ptr::ptr_hash<Shared<TypeIdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<TypeIdentifierAst>>>;

  /// Convention equality checks two types for a matching
  /// convention, and allows "&mut" to coalesce to "&", providing
  /// a way to move a borrow that should be compatible.
  SPP_EXP_FUN auto ConventionEq(
    TypeAst const &lhs_type,
    TypeAst const &rhs_type)
    -> bool;

  /// The master type equality function used for checking if
  /// two types, in their respective scopes, are semantically
  /// equal. This takes namespaces, scopes and aliases into
  /// account. There is a checker for the variants too, allowing
  /// "Some[S32]" to match "Opt[S32]" in one way (narrow into
  /// wide), and type forwarding int he same way. Generic
  /// checking is recursed into to all depths. Variadics have
  /// special handling too.
  SPP_EXP_FUN auto TypeEq(
    TypeAst const &lhs_type,
    TypeAst const &rhs_type,
    Scope const &lhs_scope,
    Scope const &rhs_scope,
    bool check_variant = true)
    -> bool;

  /// "TypeEq" on resolved types - the implementation; the written-type
  /// overload resolves both sides and asks this. The same symbol, or
  /// two instantiations under one identity key, under a compatible
  /// convention is one type; otherwise variants, "$" mocks, forwarding
  /// and the arguments each instantiation records are compared.
  SPP_EXP_FUN auto TypeEq(
    TypeRef const &lhs,
    TypeRef const &rhs,
    Scope const &lhs_scope,
    Scope const &rhs_scope,
    bool check_variant = true)
    -> bool;

  /// The overload called when generic comp arguments get compared
  /// to each other, which just reuses the expression ast normal
  /// equality methods. Provides uniformity over generic equality
  /// checking with respect to type generics.
  SPP_EXP_FUN auto TypeEq(
    ExpressionAst const &lhs_expr,
    ExpressionAst const &rhs_expr,
    Scope const &lhs_scope,
    Scope const &rhs_scope)
    -> bool;

  /// Check if two types are equal with respect to type forwarding.
  /// For example, Str forwards to &StrView, to has to be able to
  /// match the type that way around, like variants.
  SPP_EXP_FUN auto TypeFwdEq(
    TypeAst const &arg_type,
    TypeAst const &param_type,
    Scope const &arg_scope,
    Scope const &param_scope)
    -> bool;

  /// The relaxed type equality function does a normal advanced
  /// type equality check, but allows unbound generics to match
  /// against anything, and records the generic binding in the map.
  /// This uses two way binding checks, with a strict mode to
  /// enforce one way checking. Generic constraints are enforced
  /// here too.
  SPP_EXP_FUN auto RelaxedTypeEq(
    TypeAst const &lhs_type,
    TypeAst const &rhs_type,
    Scope const &lhs_scope,
    Scope const &rhs_scope,
    GenericInferenceMap &generic_args,
    bool check_variant = false,
    bool check_constraints = true)
    -> bool;

  /// A method top check and enforce the generic constraints on
  /// a single argument, given its constraints off the equivalent
  /// generic parameter.
  SPP_EXP_FUN auto EnforceGenericConstraintsOneArg(
    Vec<Shared<TypeAst>> const &constraints,
    TypeAst const &concrete_type,
    Scope const &constraints_owner_scope,
    Scope const &concrete_scope)
    -> TypeAst const*;

  /// A variant's members as types, flattened through nested
  /// variants and without duplicates: "Str or Str or S32" is
  /// "Str, S32". Members written directly keep their spelling.
  SPP_EXP_FUN auto VariantMemberTypes(
    TypeAst const &type,
    Scope const &scope)
    -> Vec<Shared<TypeAst>>;

  /// A variant's members as resolved types: flattened through nested
  /// variants and without duplicates, none for a non-variant. What a
  /// reader that needs the members' symbols, not their names, asks.
  SPP_EXP_FUN auto VariantMembers(
    TypeRef const &ref,
    Scope const &scope)
    -> Vec<TypeRef>;

  /// The template a symbol stands for where "scope" reads it ("Vec" for "Vec[Str]"), by the path its "FqName" takes:
  /// a parameter is what the scope binds it to ("Scope::Canon"), a binding or "Self" the type it names, an alias its
  /// target. A template, or a plain class, is its own.
  SPP_EXP_FUN auto TemplateOf(TypeSymbol const &sym, Scope const &scope) -> TypeSymbol*;

  /// Whether a symbol stands for the template a written type names ("Copy", or "Vec" for "Vec[Str]"), both taken to
  /// the template they stand for, rather than comparing names.
  SPP_EXP_FUN auto IsTemplate(TypeSymbol const &sym, TypeAst const &tmpl, Scope const &scope) -> bool;

}
