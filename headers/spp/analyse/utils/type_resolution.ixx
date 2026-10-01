module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_resolution;
import spp.analyse.scopes.instance_key;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeStatementAst);

namespace spp::analyse::utils::type_resolution {
  /// A type written through an alias ("Closure[(), Void]" for
  /// "type Closure[Ts, R] = FunMov[Ts, R]") as the type it stands
  /// for, arguments substituted ("FunMov[(), Void]"); any other
  /// type as it is. For readers that look at the written head,
  /// which for an alias is the alias, not its target.
  SPP_EXP_FUN auto ThroughAlias(
    TypeAst const &type,
    Scope const &scope)
    -> Shared<const TypeAst>;

  /// What a type statement names: its immediate target as written,
  /// arguments named (an alias of an alias names that alias, read
  /// through it by identity), the parameters of a target alias it
  /// passes on, the scope the class at the end of the chain was
  /// found in (where an instantiation is attached), and that class.
  /// Rejects a cyclic chain.
  SPP_EXP_FUN auto AliasStatementTarget(
    TypeStatementAst const &alias_stmt,
    bool from_use_stmt,
    Scope *tracking_scope,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*, TypeSymbol*>;

  /// Whether "ResolveWrittenType" replaces "Self" from the scope, or
  /// leaves it for a caller that decides per use what it stands for
  /// (function parameter and return types).
  SPP_EXP_CLS enum class SelfPolicy { kSubstitute, kKeep };

  /// Resolve a type as written in source: substitute "Self" and
  /// analyse, which records on each name the identity it resolves to
  /// here ("TypeAst::Written"). The written type is kept, spelling and
  /// all; readers elsewhere resolve it through that identity. A type
  /// that had "Self" replaced is analysed with abstract types allowed,
  /// as "Self" may name an abstract class.
  SPP_EXP_FUN auto ResolveWrittenType(
    TypeAst const &written,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    SelfPolicy self = SelfPolicy::kSubstitute)
    -> Shared<TypeAst>;

  /// Record on every name in a type whose meaning is fixed the
  /// identity it has in the scope the type is written in: a
  /// generic parameter, a closed class, and the template at
  /// the head of a name written with arguments. A copy read
  /// again from any other scope then resolves through the
  /// written identities ("Scope::ResolveWritten"), not by its spelling.
  SPP_EXP_FUN auto RecordWrittenParts(
    TypeAst const &type,
    Scope const &scope)
    -> void;

  /// A written type as it reads in "scope" - an instantiation's, whose
  /// bindings it is read through - as a type that means the same
  /// wherever it is read after: keyed here (each parameter read
  /// through its binding, each comp value folded) and named from that
  /// identity ("Scope::TypeAstOf"), in the written type's span. What
  /// an instantiation's own types are made from, in place of
  /// substituting its arguments into the written ones. A type that
  /// does not resolve here is kept as written. "also" binds what no
  /// binding here does (a class's own parameters, read from a "sup"
  /// block's instance).
  /// The parameters a parameter group declares, by identity, as
  /// "ParamsOf" lists a type's.
  SPP_EXP_FUN auto ParamsOfGroup(
    GenericParameterGroupAst const &params)
    -> scopes::TypeIdParams;

  /// Bind the parameters "params" lists to the arguments of the same
  /// names in "args" (an instantiation's arguments' key, as
  /// "Scope::InstanceIdentityKey" makes one), as an instantiation's
  /// arguments name the parameters of what it instantiates. With
  /// "all", nothing unless every one is bound; otherwise a parameter
  /// with no argument of its name is left out.
  SPP_EXP_FUN auto BindByName(
    scopes::TypeIdParams const &params,
    scopes::TypeId args,
    bool all)
    -> std::optional<scopes::TypeSubst>;

  /// "BindByName" for the named arguments "args", each read in
  /// "scope": what a type written in the terms of "params" is read
  /// with ("ReadWith"). A parameter with no argument is left out.
  SPP_EXP_FUN auto BindArgs(
    GenericParameterGroupAst const &params,
    Vec<GenericArgumentAst*> const &args,
    Scope const &scope)
    -> scopes::TypeSubst;

  /// A type written in "written_scope" (in the terms of parameters
  /// "subst" binds) with them bound, named from that identity for
  /// "scope". A type that does not resolve is kept as written.
  SPP_EXP_FUN auto ReadWith(
    TypeAst const &written,
    Scope const &written_scope,
    scopes::TypeSubst const &subst,
    Scope const &scope)
    -> Shared<TypeAst>;

  SPP_EXP_FUN auto ReadInto(
    TypeAst const &written,
    Scope const &scope,
    scopes::TypeSubst const &also = {})
    -> Shared<TypeAst>;
}
