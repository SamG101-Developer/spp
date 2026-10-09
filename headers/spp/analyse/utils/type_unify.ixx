module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_unify;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.type_key;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);

namespace spp::analyse::utils::type_unify {
  /// Pattern matching by identity: whether "given" fits "declared",
  /// binding the parameters "declared" names into "bindings" by
  /// their "ParamId". A parameter binds what is opposite it, the
  /// same each time it is named; an instance's arguments are matched
  /// by the parameter each is for ("FindArgOf"), a tuple's by
  /// position, a trailing variadic parameter taking the rest as
  /// one tuple (or comp pack). "check_variant" lets a variant
  /// pattern take any of its members; "check_constraints" holds a
  /// bound type to its parameter's constraints.
  SPP_EXP_FUN auto UnifyTypeIds(
    TypeId given, TypeId declared, Scope const &given_scope, Scope const &declared_scope,
    GenericSubst &bindings, bool check_variant = false, bool check_constraints = true) -> bool;

  /// "UnifyTypeIds" for a comp value: a comp parameter binds
  /// the value opposite it, a pack matches element by element
  /// (a trailing variadic parameter taking the rest), and
  /// anything else is the same value by identity.
  SPP_EXP_FUN auto UnifyCompIds(
    CompId given, CompId declared, GenericSubst &bindings) -> bool;
}
