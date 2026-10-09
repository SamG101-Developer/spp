module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.substitution;
import spp.analyse.scopes.type_key;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::scopes {
  /// What a substitution rewrites in a "TypeId" or a "CompId":
  /// type and comp parameters by "ParamId" (0 standing for
  /// "Self", which a key spells rather than identifies), each
  /// to what it is bound to.
  SPP_EXP_CLS struct GenericSubst {
    std::vector<std::pair<std::uint64_t, TypeId>> TypeParams;
    std::vector<std::pair<std::uint64_t, CompId>> CompParams;
    std::vector<std::uint64_t> TypePackParams;
    std::vector<std::uint64_t> CompPackParams;

    SPP_ATTR_NODISCARD auto IsEmpty() const -> bool {
      return TypeParams.empty() and CompParams.empty();
    }
  };

  /// How an expression carried out of the scope that wrote it
  /// is read: each type and comp parameter it names is keyed
  /// in "Written", where it is the parameter it names, rewritten
  /// with "Bindings", and read back in "Reading". A default read
  /// in an instantiation's own scope, whose bindings are
  /// registered there, needs no "Bindings": it is written and
  /// read in that scope.
  SPP_EXP_CLS struct ExprSubst {
    GenericSubst Bindings;
    Scope const *Written = nullptr;
    Scope const *Reading = nullptr;

    /// Written and read in one scope (an instantiation's, whose
    /// bindings are registered there), with "bindings" applied
    /// on top.
    static auto In(
      Scope const &scope, GenericSubst bindings = {}) -> ExprSubst;

    /// Written in "written" (in the terms of the parameters
    /// "bindings" binds), read in "reading".
    static auto Across(
      Scope const &written, GenericSubst bindings, Scope const &reading) -> ExprSubst;
  };

  /// Substitute the generics in "id", using the "subst" struct,
  /// creating a new type id. Memoised.
  SPP_EXP_FUN SPP_ATTR_HOT auto SubstituteTypeId(
    TypeId id, GenericSubst const &subst) -> TypeId;

  /// Substitute the generics in "id", using the "subst" struct,
  /// creating a new comp id. Memoised.
  SPP_EXP_FUN auto SubstituteCompId(
    CompId id, GenericSubst const &subst) -> CompId;

  /// The parameters a parameter group declares, by identity,
  /// as "ParamsNamedBy" lists a type's.
  SPP_EXP_FUN auto ParamsDeclaredBy(GenericParameterGroupAst const &params) -> TypeIdParams;

  /// Bind the parameters "params" lists to the arguments of the
  /// same names in "args" (an instantiation's arguments' key, as
  /// "Scope::ArgsIdOf" makes one), as an instantiation's
  /// arguments name the parameters of what it instantiates. With
  /// "all", nothing unless every one is bound; otherwise a
  /// parameter with no argument of its name is left out.
  SPP_EXP_FUN auto BindByName(TypeIdParams const &params, TypeId args, bool all) -> std::optional<GenericSubst>;

  /// "BindByName" for the named arguments "args", each read in
  /// "scope": what a type written in the terms of "params" is read
  /// with ("ReadType"). A parameter with no argument is left out.
  SPP_EXP_FUN auto BindArgs(
    GenericParameterGroupAst const &params, Vec<GenericArgumentAst*> const &args,
    Scope const &scope) -> GenericSubst;

  /// The part of "bindings" that binds a parameter "params"
  /// declares: what a match says about one declaration's own
  /// parameters, when its pattern is read where others' are
  /// visible too (an instantiated block's, whose arguments name
  /// the caller's parameters).
  SPP_EXP_FUN auto BindingsFor(
    GenericSubst const &bindings, GenericParameterGroupAst const &params) -> GenericSubst;

  /// Bind "Self" in "subst" (parameter 0) to "self_type", read
  /// in "scope". Only where "Self" is meant to be read as what
  /// a use pinned it to (a default carried to the use); a type
  /// argument otherwise keeps "Self" as written.
  SPP_EXP_FUN auto BindSelf(
    GenericSubst &subst, TypeAst const &self_type, Scope const &scope) -> void;

  /// What an instantiation binds its class's parameters to, read
  /// off its identity (defaults filled), as a type written in the
  /// class's terms is read with. Empty for anything that is not
  /// a class instance.
  SPP_EXP_FUN auto InstanceBindings(
    TypeRef const &inst) -> GenericSubst;

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
