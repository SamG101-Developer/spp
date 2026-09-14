module;
#include <spp/macros.hpp>

export module spp.analyse.utils.generic_bindings;
import spp.analyse.utils.type_compare;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::utils::generic_bindings, struct GenericBinding);
use(spp::analyse::utils::generic_bindings, class GenericBindingSet);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

/// What one generic parameter is bound to. The "Type" and
/// "Comp" are mutually exclusive; only one is ever set.
SPP_EXP_CLS struct spp::analyse::utils::generic_bindings::GenericBinding {
  Shared<TypeAst> Type;
  ExpressionAst *Comp;

  SPP_ATTR_NODISCARD auto IsType() const -> bool;
  SPP_ATTR_NODISCARD auto IsBound() const -> bool;
};

/// The bindings for a group of generic parameters. A parameter
/// may be offered more than one candidate argument, so they
/// are accumulated and reconciled by the conflict enforcement,
/// allowing disagreements error, otherwise coalesce.
SPP_EXP_CLS class spp::analyse::utils::generic_bindings::GenericBindingSet {
public:
  GenericBindingSet();

  /// The set owns the arguments it was built from, so it moves
  /// rather than copies. All copy constructors are deleted.
  GenericBindingSet(GenericBindingSet const &that) = delete;
  GenericBindingSet(GenericBindingSet &&that) noexcept = default;
  ~GenericBindingSet();
  auto operator=(GenericBindingSet const &that) -> GenericBindingSet& = delete;
  auto operator=(GenericBindingSet &&that) noexcept -> GenericBindingSet& = default;

  /// Take the bindings a written argument group states, moving
  /// the arguments into the set. The group is already named
  /// from a prior NameGnArgs.
  static auto FromNamedArgs(
    GenericArgumentGroupAst &a_group,
    ScopeManager &sm) -> GenericBindingSet;

  /// Provide a view over the raw pointers of the generic argument
  /// unique pointers.
  SPP_ATTR_NODISCARD auto Args() const
    -> Vec<GenericArgumentAst*>;

  /// Add a new type generic into the internal set, providing a
  /// new candidate "TypeAst" to a "TypeIdentifierAst" key.
  auto Add(
    Shared<TypeIdentifierAst> const &name,
    Shared<TypeAst> value)
    -> void;

  /// Add a new comp generic into the internal set, providing a
  /// new candidate "ExpressionAst" to a "TypeIdentifierAst" key.
  auto Add(
    Shared<TypeIdentifierAst> const &name,
    ExpressionAst *value)
    -> void;

  /// Replace all the candidates of a "TypeIdentifierAst" key.
  /// This is used once the "winning" candidate has been
  /// determined.
  auto Replace(
    TypeIdentifierAst const *name,
    Shared<TypeAst> value)
    -> void;

  /// Check whether is candidate type generic name is present
  /// and has candidates.
  SPP_ATTR_NODISCARD auto ContainsType(
    TypeIdentifierAst const *name) const
    -> bool;

  /// Check whether is candidate comp generic name is present
  /// and has candidates.
  SPP_ATTR_NODISCARD auto ContainsComp(
    TypeIdentifierAst const *name) const
    -> bool;

  /// What a parameter resolved to - its first candidate, which
  /// "EnforceNoConflicts" has established the others agree with.
  SPP_ATTR_NODISCARD auto Resolved(
    TypeIdentifierAst const *name) const
    -> GenericBinding;

  /// View of the generic type argument names as raw pointers.
  SPP_ATTR_NODISCARD auto TypeNames() const
    -> Vec<Shared<TypeIdentifierAst>>;

  /// View of the generic comp argument names as raw pointers.
  SPP_ATTR_NODISCARD auto CompNames() const
    -> Vec<Shared<TypeIdentifierAst>>;

  /// Check every parameter's candidates agree with each other,
  /// and raise on the first that does not.
  auto EnforceNoConflicts(
    ScopeManager &sm) const
    -> void;

  /// The resolved bindings in the form the type substitution
  /// machinery takes.
  SPP_ATTR_NODISCARD auto ToInferenceMap() const
    -> type_compare::GenericInferenceMap;

  /// The resolved bindings as generic arguments, ordered to match
  /// the parameter declarations. Order matters because the argument
  /// list ends up in a type's name, and two spellings of one
  /// instantiation have to mangle alike.
  SPP_ATTR_NODISCARD auto ToArgs(
    GenericParameterGroupAst const &p_group) const
    -> Vec<Unique<GenericArgumentAst>>;

private:
  struct _Candidates {
    Vec<Shared<TypeAst>> Types;
    Vec<ExpressionAst*> Comps;
  };

  Vec<Unique<GenericArgumentAst>> _OwnedArgs;

  Map<
    Shared<TypeIdentifierAst>, _Candidates,
    spp::utils::ptr::ptr_hash<Shared<TypeIdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<TypeIdentifierAst>>> _Table;
};

namespace spp::analyse::utils::generic_bindings {
  SPP_EXP_CLS
  using InferenceSourceMap = Map<
    Shared<IdentifierAst>,
    Shared<TypeAst>,
    spp::utils::ptr::ptr_hash<Shared<IdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<IdentifierAst>>>;

  SPP_EXP_CLS
  using InferenceTargetMap = Map<
    Shared<IdentifierAst>,
    Shared<TypeAst>,
    spp::utils::ptr::ptr_hash<Shared<IdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<IdentifierAst>>>;

  /// Given the constraints on the generic parameters, ensure
  /// that the corresponding generic arguments satisfy the
  /// constraints. Also handles the cross-application of
  /// generics into the constraints that themselves rely on
  /// these generics.
  SPP_EXP_FUN auto EnforceGenericConstraintsAllArgs(
    GenericParameterGroupAst const &p_group,
    GenericArgumentGroupAst const &a_group,
    Scope const &owner_scope,
    ScopeManager &sm,
    meta::CompilerMetaData &meta)
    -> void;

  /// Massive method to infer generics from a source into a
  /// target, based on generic arguments and parameters. Handles
  /// type vs comp generics, constraints, defaults, variadics,
  /// cross-application, and specific tuple handling too.
  SPP_EXP_FUN auto InferGnArgs(
    GenericParameterGroupAst const &p_group,
    GenericArgumentGroupAst &a_group,
    Shared<InferenceSourceMap> infer_source,
    Shared<InferenceTargetMap> infer_target,
    Shared<Ast> const &owner,
    Scope const &owner_scope,
    Shared<IdentifierAst> const &variadic_fn_param_name,
    bool is_tuple_owner,
    ScopeManager &sm,
    meta::CompilerMetaData &meta)
    -> void;

  /// Take a generic argument group, and name the arguments
  /// based on the parameters available. Custom logic for
  /// optional and variadic parameters.
  SPP_EXP_FUN auto NameGnArgs(
    GenericArgumentGroupAst &a_group,
    GenericParameterGroupAst const &p_group,
    Ast const &owner,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    bool is_tuple_owner = false)
    -> void;

  /// Simple helper method to detect whether a generic is
  /// binding to itself, ie a "T=T" or "n=n" part. Required
  /// for filtering some generics out of analysis that will
  /// be substituted later.
  SPP_EXP_FUN auto BindsToItself(
    GenericArgumentAst const &arg)
    -> bool;

  /// Strip the type down to a non-generic version if any
  /// of the generics don't bind to itself. Needed for
  /// qualification steps. Todo: preferable this goes.
  SPP_EXP_FUN auto WithoutSelfBindingGenerics(
    Shared<TypeAst> const &type)
    -> Shared<TypeAst>;
}
