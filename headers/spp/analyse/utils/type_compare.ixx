module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_compare;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
}

namespace spp::analyse::utils::type_compare {
  SPP_EXP_CLS
  using GenericInferenceMap = Map<
    Shared<asts::TypeIdentifierAst>, asts::ExpressionAst*,
    spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>>;

  SPP_EXP_FUN auto ConventionEq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type)
    -> bool;

  /**
   * The symbolic equality type checker is a complex type checking algorithm that takes namespacing, scopes, aliases,
   * variants, etc, all into account, and returns whether two types are indeed the same. Generic arguments are also
   * taken into account, as-well as the variadic generic variation.
   *
   * Variant type matching allows the rhs to match against the lhs side, when the lhs is a variant, such as
   * @c {Opt[T]}, and the rhs is a inner type to the variant, such as @c {Some[T]}, or @c {None}.
   *
   * The type checking of generic types, such as @c {Vec[Str]} vs @c {Vec[Str]}, require the generic arguments to be
   * symbolically equal too. A recursive algorithm is used to inspect arguments. However, non-type generic arguments
   * (comp generic arguments) can't be "type-compared", so their values are compared instead. See the other overload
   * of this function.
   *
   * @param lhs_type The left hand side type to compare.
   * @param rhs_type The right hand side type to compare.
   * @param lhs_scope The scope to identify the lhs type in. This is used to resolve aliases and namespaces.
   * @param rhs_scope The scope to identify the rhs type in. This is used to resolve aliases and namespaces.
   * @param check_variant Whether to allow "variant matches" for types.
   * @return If the two types are symbolically equal, meaning they are the same type, or one is a variant of the
   * other.
     */
  SPP_EXP_FUN auto TypeEq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    bool check_variant = true)
    -> bool;

  SPP_EXP_FUN auto TypeEq(
    asts::ExpressionAst const &lhs_expr,
    asts::ExpressionAst const &rhs_expr,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope)
    -> bool;

  SPP_EXP_FUN auto TypeFwdEq(
    asts::TypeAst const &arg_type,
    asts::TypeAst const &param_type,
    scopes::Scope const &arg_scope,
    scopes::Scope const &param_scope)
    -> bool;

  /**
   * @param strict_generic_args Whether a generic @e argument that is still an unbound parameter fails to match a type
   * written opposite it. Off for the argument/parameter checks, which is the whole point of them - a "T" parameter
   * accepts the "U8" argument offered for it. On when deciding whether a @c sup block applies to a type: an unbound
   * "T" is not yet anything, so letting it match attaches @c "sup NonNull[U8]" to the template @c "NonNull[T]" and
   * lets a body written for every "T" call what only "NonNull[U8]" has - resolving while "T" stands for nothing, and
   * failing once it stands for something, against code the author cannot see.
   *
   * @note Only the arguments are held to this, never the bare parameter itself. @c "A" compared against @c "Alloc" is
   * how a constraint's @c sup block is attached to @c "A" to begin with, so refusing that match would leave every
   * constrained parameter with none of the members its constraint gives it.
   */
  SPP_EXP_FUN auto RelaxedTypeEq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    GenericInferenceMap &generic_args,
    bool check_variant = false,
    bool check_constraints = true,
    bool strict_generic_args = false)
    -> bool;

  SPP_EXP_FUN auto RelaxedTypeEq(
    asts::ExpressionAst const &lhs_expr,
    asts::ExpressionAst const &rhs_expr,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    GenericInferenceMap &generic_args)
    -> bool;

  /**
   * Check that @p concrete_type (and its supertypes) satisfy every constraint in @p constraints. Returns the first
   * unsatisfied constraint, or @c nullptr if they are all satisfied. This is a non-throwing check so that hot callers
   * (eg @c ConstraintEq via @c TypeEq) can branch on the result without paying for exception machinery; callers that
   * want an error should raise @c SppGenericConstraintError from the returned constraint.
   */
  SPP_EXP_FUN auto EnforceGenericConstraintsOneArg(
    Vec<Shared<asts::TypeAst>> const &constraints,
    asts::TypeAst const &concrete_type,
    scopes::Scope const &constraints_owner_scope,
    scopes::Scope const &concrete_scope)
    -> asts::TypeAst const*;

  SPP_EXP_FUN auto DedupVariableInnerTypes(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Vec<Shared<asts::TypeAst>>;
}
