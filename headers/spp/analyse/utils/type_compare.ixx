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
  /// What each generic a match bound is bound to. A value is held,
  /// not borrowed: it can be built by the match (a trailing pack's
  /// tuple), and otherwise keeps the type it was read from alive.
  SPP_EXP_CLS using GenericInferenceMap = Map<
    Shared<TypeIdentifierAst>, Shared<ExpressionAst>,
    spp::utils::ptr::ptr_hash<Shared<TypeIdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<TypeIdentifierAst>>>;

  /// Convention equality checks two types for a matching
  /// convention, and allows "&mut" to coalesce to "&", providing
  /// a way to move a borrow that should be compatible.
  SPP_EXP_FUN auto ConventionEq(
    TypeAst const &lhs_type, TypeAst const &rhs_type) -> bool;

  /// The much stricter version of "Assignable". This checks
  /// if the 2 types are the exact same type: the same type,
  /// the same convention, with recursively checked generics.
  /// Namespaces, aliases, bindings and "Self" are managed
  /// here, and nothing converts.
  SPP_EXP_FUN auto TypeEq(
    TypeAst const &lhs_type, TypeAst const &rhs_type, Scope const &lhs_scope, Scope const &rhs_scope) -> bool;

  /// "TypeEq" on resolved types.
  SPP_EXP_FUN auto TypeEq(
    TypeRef const &lhs, TypeRef const &rhs, Scope const &lhs_scope, Scope const &rhs_scope) -> bool;

  /// "TypeEq" for comp values: the same value by identity, each read where it is written ("1_uz + 1_uz" is "2_uz").
  SPP_EXP_FUN auto CompEq(
    ExpressionAst const &lhs, ExpressionAst const &rhs, Scope const &lhs_scope, Scope const &rhs_scope) -> bool;

  /// Whether we can use "value_type" where "target_type" is expected,
  /// according to the following rules:
  ///   - they are the same type OR
  ///   - we are using the ! type which fits anywhere OR
  ///   - we are coercing &mut to &, and the borrowees are assignable OR
  ///   - a narrower type of a variant (concrete or sub-variant) OR
  ///   - a narrower type of a variant within a generic (Opt[Some[T]] vs Opt[Opt[T]]) OR
  ///   - a $mock type vs a FunXXX family overload is superimposes OR
  ///   - a type-forwarding cast: Vec[T] -> &View[T]
  SPP_EXP_FUN auto Assignable(
    TypeAst const &target_type, TypeAst const &value_type, Scope const &target_scope, Scope const &value_scope) -> bool;

  /// The identical rules to above, but for type reference types, which
  /// are used as much as possible.
  SPP_EXP_FUN auto Assignable(
    TypeRef const &target, TypeRef const &value, Scope const &target_scope, Scope const &value_scope) -> bool;


  /// Check if two types are equal with respect to type forwarding.
  /// For example, Str forwards to &StrView, to has to be able to
  /// match the type that way around, like variants.
  SPP_EXP_FUN auto TypeFwdEq(
    TypeAst const &arg_type, TypeAst const &param_type, Scope const &arg_scope, Scope const &param_scope) -> bool;

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

  /// The first of a generic parameter's constraints a resolved
  /// argument does not satisfy, or null: "T: Copy" with "T=Str"
  /// asks whether "Str" is superimposed with "Copy". A type
  /// written "Self" ("is_self") also answers for its own class,
  /// where "Self" is not generic. None for no type (a "$" closure type can resolve to nothing).
  SPP_EXP_FUN auto UnmetConstraint(
    Vec<Shared<TypeAst>> const &constraints,
    TypeRef const &concrete,
    bool is_self,
    Scope const &constraints_owner_scope,
    Scope const &concrete_scope)
    -> TypeAst const*;

  /// A variant's members in their one canonical order ("Bool or S32"
  /// is "S32 or Bool"): by the qualified name of what each names (the
  /// symbol given with it), else by its spelling, ties kept in order.
  /// The analysed variant ("TypeIdentifierAst::Stage7") and the one
  /// built from an identity ("Scope::TypeAstOf") are both put in it,
  /// so their members' tags agree.
  SPP_EXP_FUN auto OrderVariantMembers(
    Vec<Pair<Shared<TypeAst>, TypeSymbol const*>> members)
    -> Vec<Shared<TypeAst>>;

  /// A variant's members as types, flattened through nested
  /// variants and without duplicates: "Str or Str or S32" is
  /// "Str, S32". Members written directly keep their spelling.
  SPP_EXP_FUN auto VariantMembers(
    TypeAst const &type,
    Scope const &scope)
    -> Vec<Shared<TypeAst>>;

  /// A variant's members as resolved types: flattened through nested
  /// variants and without duplicates, none for a non-variant. What a
  /// reader that needs the members' symbols, not their names, asks.
  SPP_EXP_FUN auto VariantMemberRefs(
    TypeRef const &ref,
    Scope const &scope)
    -> Vec<TypeRef>;
}
