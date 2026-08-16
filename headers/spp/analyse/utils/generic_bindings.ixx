module;
#include <spp/macros.hpp>

export module spp.analyse.utils.generic_bindings;
import spp.analyse.utils.type_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::analyse::utils::generic_bindings {
  using InferenceSourceMap = Map<
    Shared<asts::IdentifierAst>,
    Shared<asts::TypeAst>,
    spp::utils::ptr::ptr_hash<Shared<asts::IdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<asts::IdentifierAst>>>;

  using InferenceTargetMap = Map<
    Shared<asts::IdentifierAst>,
    Shared<asts::TypeAst>,
    spp::utils::ptr::ptr_hash<Shared<asts::IdentifierAst>>,
    spp::utils::ptr::ptr_eq<Shared<asts::IdentifierAst>>>;

  SPP_EXP_FUN auto EnforceNoUninferredGnArgs(
    Vec<Shared<asts::TypeIdentifierAst>> const &p_names,
    Vec<Shared<asts::TypeIdentifierAst>> const &i_names,
    scopes::Scope const &owner_scope,
    Shared<asts::Ast> const &owner,
    scopes::ScopeManager &sm)
    -> void;


  SPP_EXP_FUN auto EnforceGenericConstraintsAllArgs(
    asts::GenericParameterGroupAst const &p_group,
    asts::GenericArgumentGroupAst const &a_group,
    scopes::Scope const &owner_scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void;


  SPP_EXP_FUN auto InferGnArgs(
    asts::GenericParameterGroupAst const &p_group,
    asts::GenericArgumentGroupAst &a_group,
    InferenceSourceMap infer_source,
    InferenceTargetMap infer_target,
    Shared<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    Shared<asts::IdentifierAst> const &variadic_fn_param_name,
    bool is_tuple_owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void;


  /**
   * Rewrite a generic argument group into its canonical form: every generic argument as a "keyword" generic argument,
   * bound against the parameter it 's for, ordered as the parameters are declared. Mutates in place.
   * @param a_group The argument group to rewrite in place.
   * @param p_group The parameters the arguments were written against.
   * @param owner The ast the parameters belong to (for errors).
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   * @param is_tuple_owner Whether the owner is a tuple, whose arguments are left alone to stop an infinite recursion.
   */
  SPP_EXP_FUN auto NameGnArgs(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta,
    bool is_tuple_owner = false)
    -> void;

  /**
   * Whether a generic argument restates its parameter rather than binding it to anything. Inference produces these
   * whenever a generic body calls something that shares one of its generics: "BigUInt::from(that)" written inside
   * "sup [cmp w: U32] BigInt ext From[SizedIntegerUnsigned[w]]" pins "w" to "w", because "that" is declared in terms
   * of the very parameter being inferred. Such an argument has to count as inferred - the parameter is accounted for -
   * but it must not count as a substitution, because an instantiation built from it would be the template with an
   * empty parameter list, and an empty parameter list is exactly what marks a prototype as no longer a template.
   * @param arg The generic argument to inspect.
   * @return Whether the argument binds its parameter to the parameter itself.
   */
  SPP_EXP_FUN auto BindsToItself(
    asts::GenericArgumentAst const &arg)
    -> bool;

  /**
   * What one generic parameter is bound to. The `Type` and `Comp` are mutually exclusive; only one is ever set.
   */
  SPP_EXP_CLS struct GenericBinding {
    Shared<asts::TypeAst> Type;
    asts::ExpressionAst *Comp;

    SPP_ATTR_NODISCARD auto IsType() const -> bool;
    SPP_ATTR_NODISCARD auto IsBound() const -> bool;
  };

  /**
   * The bindings for a group of generic parameters. A parameter may be offered more than one candidate - the same
   * generic can be reached through several arguments, or through an argument and a constraint - so candidates are
   * accumulated and only reconciled by @c EnforceNoConflicts , which is what makes "two arguments disagree about T" an
   * error rather than a silent last-write-wins.
   */
  SPP_EXP_CLS class GenericBindingSet {
  public:
    GenericBindingSet();

    /**
     * The set owns the arguments it was built from, so it moves rather than copies.
     */
    GenericBindingSet(GenericBindingSet const &that) = delete;
    GenericBindingSet(GenericBindingSet &&that) noexcept = default;
    ~GenericBindingSet();
    auto operator=(GenericBindingSet const &that) -> GenericBindingSet& = delete;
    auto operator=(GenericBindingSet &&that) noexcept -> GenericBindingSet& = default;

    /**
     * Take the bindings a written argument group states, moving the arguments into the set. The group must already be
     * named (see @c NameGnArgs ): each binding is attached to a parameter name, and an argument that is still
     * positional does not name anything.
     * @param a_group The argument group to take the bindings from, left empty.
     * @param sm The scope manager, for the error if an argument is not named.
     * @return The bindings the group stated.
     */
    static auto FromNamedArgs(
      asts::GenericArgumentGroupAst &a_group,
      scopes::ScopeManager &sm) -> GenericBindingSet;

    /**
     * The arguments this set was built from and now owns. Same behaviour as @c GenericArgumentGroup::GetAllArgs() and
     * @c GenericArgumentGroup::GetKeywordArgs() as all the arguments are keyword based.
     */
    SPP_ATTR_NODISCARD auto Args() const
      -> Vec<asts::GenericArgumentAst*>;

    /**
     * Offer a type as a candidate for a parameter.
     * @param name The generic parameter's name.
     * @param value The type it is being bound to.
     */
    auto Add(
      Shared<asts::TypeIdentifierAst> const &name,
      Shared<asts::TypeAst> value)
      -> void;

    /**
     * Offer a comp-time value as a candidate for a parameter.
     * @param name The generic parameter's name.
     * @param value The expression it is being bound to.
     */
    auto Add(
      Shared<asts::TypeIdentifierAst> const &name,
      asts::ExpressionAst *value)
      -> void;

    /**
     * Replace what a parameter resolved to, discarding its other candidates. Used once the winning candidates are
     * known and are being rewritten in terms of each other.
     * @param name The generic parameter's name.
     * @param value The type it now resolves to.
     */
    auto Replace(
      asts::TypeIdentifierAst const *name,
      Shared<asts::TypeAst> value)
      -> void;

    /**
     * Whether a parameter has been offered a type candidate.
     * @param name The generic parameter's name.
     */
    SPP_ATTR_NODISCARD auto ContainsType(
      asts::TypeIdentifierAst const *name) const
      -> bool;

    /**
     * Whether a parameter has been offered a comp-time candidate.
     * @param name The generic parameter's name.
     */
    SPP_ATTR_NODISCARD auto ContainsComp(
      asts::TypeIdentifierAst const *name) const
      -> bool;

    /**
     * What a parameter resolved to - its first candidate, which @c EnforceNoConflicts has established the others agree
     * with.
     * @param name The generic parameter's name.
     */
    SPP_ATTR_NODISCARD auto Resolved(
      asts::TypeIdentifierAst const *name) const
      -> GenericBinding;

    SPP_ATTR_NODISCARD auto TypeNames() const
      -> Vec<Shared<asts::TypeIdentifierAst>>;

    SPP_ATTR_NODISCARD auto CompNames() const
      -> Vec<Shared<asts::TypeIdentifierAst>>;

    /**
     * Check every parameter's candidates agree with each other, and raise on the first that does not.
     * @param sm The scope manager, for the type comparison and the error.
     */
    auto EnforceNoConflicts(
      scopes::ScopeManager &sm) const
      -> void;

    /**
     * The resolved bindings in the form the type substitution machinery takes.
     */
    SPP_ATTR_NODISCARD auto ToInferenceMap() const
      -> type_utils::GenericInferenceMap;

    /**
     * The resolved bindings as generic arguments, ordered to match the parameter declarations. Order matters because
     * the argument list ends up in a type's name, and two spellings of one instantiation have to mangle alike.
     * @param p_group The parameter group the bindings are for.
     */
    SPP_ATTR_NODISCARD auto ToArgs(
      asts::GenericParameterGroupAst const &p_group) const
      -> Vec<Unique<asts::GenericArgumentAst>>;

  private:
    struct _Candidates {
      Vec<Shared<asts::TypeAst>> Types;
      Vec<asts::ExpressionAst*> Comps;
    };

    /**
     * The written arguments, kept alive because a comp binding borrows the expression inside one.
     */
    Vec<Unique<asts::GenericArgumentAst>> _OwnedArgs;

    Map<
      Shared<asts::TypeIdentifierAst>, _Candidates,
      spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
      spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>> _Table;
  };
}
