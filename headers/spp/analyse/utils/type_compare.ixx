module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_compare;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::type_compare {
  /// Convention equality checks two types for a matching
  /// convention, and allows "&mut" to coalesce to "&", providing
  /// a way to move a borrow that should be compatible. The
  /// conventions as written: a binding's own is not read.
  SPP_EXP_FUN auto ConventionEq(
    TypeAst const &lhs_type, TypeAst const &rhs_type) -> bool;

  /// The much stricter version of "Assignable". This checks
  /// if the 2 types are the exact same type: the same type,
  /// the same convention, with recursively checked generics.
  /// Namespaces, aliases, bindings and "Self" are managed
  /// here, and nothing converts.
  SPP_EXP_FUN auto TypeEq(
    TypeRef const &lhs, TypeRef const &rhs,
    Scope const &lhs_scope, Scope const &rhs_scope) -> bool;

  /// Whether we can use "value_type" where "target_type" is expected,
  /// according to the following rules:
  ///   - they are the same type OR
  ///   - we are using the ! type which fits anywhere OR
  ///   - we are coercing &mut to &, and the borrowees are assignable
  /// OR
  ///   - a narrower type of a variant (concrete or sub-variant) OR
  ///   - a narrower type of a variant within a generic (Opt[Some[T]]
  /// vs Opt[Opt[T]]) OR
  ///   - a $mock type vs a FunXXX family overload is superimposes OR
  ///   - a type-forwarding cast: Vec[T] -> &View[T]
  SPP_EXP_FUN auto Assignable(
    TypeRef const &target, TypeRef const &value,
    Scope const &target_scope, Scope const &value_scope) -> bool;

  /// Check if two types are equal with respect to type forwarding.
  /// For example, Str forwards to &StrView, to has to be able to
  /// match the type that way around, like variants.
  SPP_EXP_FUN auto TypeFwdEq(
    TypeRef const &arg, TypeRef const &param,
    Scope const &arg_scope, Scope const &param_scope) -> bool;

  /// The first of a generic parameter's constraints a resolved
  /// argument does not satisfy, or null: "T: Copy" with "T=Str"
  /// asks whether "Str" is superimposed with "Copy". A type
  /// written "Self" ("is_self") also answers for its own class,
  /// where "Self" is not generic. None for no type (a "$" closure
  // type can resolve to nothing).
  SPP_EXP_FUN auto UnmetConstraint(
    Vec<Shared<TypeAst>> const &constraints, TypeRef const &concrete,
    bool is_self, Scope const &constraints_owner_scope, Scope const &concrete_scope)
    -> TypeAst const*;
}
