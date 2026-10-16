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
    Vec<Pair<std::uint64_t, TypeId>> TypeParams;
    Vec<Pair<std::uint64_t, CompId>> CompParams;
    Vec<std::uint64_t> TypePackParams;
    Vec<std::uint64_t> CompPackParams;

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

  /// Forget every remembered substitution ("SubstituteTypeId",
  /// "SubstituteCompId"), keyed by identities that "ClearTypeIds"
  /// and "ClearCompIds" free: called with them.
  SPP_EXP_FUN auto ClearSubstitutionMemos() -> void;

  /// The parameters a parameter group declares, by identity,
  /// as "ParamsNamedBy" lists a type's.
  SPP_EXP_FUN auto ParamsDeclaredBy(GenericParameterGroupAst const &params) -> TypeIdParams;

  /// "BindingsOfArgs" for the named arguments "args", each read in
  /// "scope": what a type written in the terms of "params" is read
  /// with ("ReadType"). A parameter with no argument is left out.
  SPP_EXP_FUN auto BindArgs(
    GenericParameterGroupAst const &params, Vec<GenericArgumentAst*> const &args,
    Scope const &scope) -> GenericSubst;

  /// What the arguments whose identity is "args" ("Scope::ArgsIdOf")
  /// bind, by the parameter each is for (packs marked); one keyed
  /// by spelling or position binds nothing. With "only", just the
  /// parameters it lists, in its order (an instantiation's
  /// arguments binding the parameters of what it instantiates); a
  /// parameter with no argument is left out. The inverse of
  /// "ArgsIdOfBindings".
  SPP_EXP_FUN auto BindingsOfArgs(
    TypeId args, TypeIdParams const *only = nullptr) -> GenericSubst;

  /// The identity of "bindings" as arguments ("Scope::ArgsIdOf"
  /// keys arguments named for the parameters they bind): each
  /// under its parameter, types then comp values, "Self" (0)
  /// left out, each value read where "scope" reads it (a
  /// parameter it names that "scope" binds, as what it binds:
  /// "Scope::ReadIn"). Null when there is nothing bound.
  SPP_EXP_FUN auto ArgsIdOfBindings(
    GenericSubst const &bindings, Scope const &scope) -> TypeId;

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

  /// "type" with "Self" (parameter 0) read as "self", by identity:
  /// "type" itself when either is null or "type" names no "Self".
  SPP_EXP_FUN auto SubstituteSelfId(
    TypeId type, TypeId self) -> TypeId;

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

}
