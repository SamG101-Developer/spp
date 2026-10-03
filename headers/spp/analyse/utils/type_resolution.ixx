module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_resolution;
import spp.analyse.scopes.instance_key;
import spp.analyse.utils.type_compare;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::type_resolution {
  /// Whether a type names a generic parameter, by identity: a part (or a comp name) that is that parameter where it is
  /// read in "scope" ("Vec[T]" names "T"; a "T" of another declaration does not). Used to find a "sup" block's
  /// parameters its type never names (so never inferable), and the constraints others are inferred through.
  SPP_EXP_FUN auto DoesTypeNameAGnParam(
    TypeAst const &type,
    GenericParameterAst const &param,
    Scope const &scope)
    -> bool;



  /// Whether "AnalyseWrittenType" replaces "Self" from the scope, or
  /// leaves it for a caller that decides per use what it stands for
  /// (function parameter and return types).
  SPP_EXP_CLS enum class SelfPolicy { kSubstitute, kKeep };

  /// A type analysis pipe and symbol retrieval. Handles all
  /// the common analysis for a type - checking it exists,
  /// qualifying, Self handling, general analysis etc.
  SPP_EXP_FUN auto AnalyseWrittenType(
    TypeAst const &written, ScopeManager &sm, meta::CompilerMetaData &meta,
    SelfPolicy self = SelfPolicy::kSubstitute) -> Shared<TypeAst>;

  /// "AnalyseWrittenType" for a comp value written here: the value it folds to, else the value of the comp generic it
  /// names ("SizedInteger[w]" in a "[cmp w: U32]" instance stands for 32), its name read by identity
  /// ("Scope::FindWrittenVarSymbol", through "FindVarSymbol"). Null when neither, and it stays as written.
  SPP_EXP_FUN auto AnalyseWrittenComp(
    ExpressionAst const &written,
    Scope const &scope)
    -> Unique<ExpressionAst>;

  /// Record on a written type the identity "sym" (what it names) stands for wherever it is read
  /// ("WrittenTypeIdOf"), unless it records one already. The one rule for every type stamped: a parameter is that
  /// parameter, a binding the parameter it binds; an alias is itself (read through to its target where read, so one
  /// stamped before its target resolves is not frozen to the target's template); anything else is what it is. A bare
  /// template records nothing: it is not a type until its arguments are filled.
  SPP_EXP_FUN auto RecordWrittenType(
    TypeAst const &type,
    TypeSymbol const &sym)
    -> void;

  /// "RecordWrittenType" for a comp name: a comp parameter, or a binding of one, records that parameter
  /// ("IdentifierAst::WrittenCompParamId").
  SPP_EXP_FUN auto RecordWrittenComp(
    IdentifierAst const &name,
    VariableSymbol const &sym)
    -> void;

  /// Record on every part of a type what it names here ("RecordWrittenType", "RecordWrittenComp"): "Vec[T]" records
  /// "T"'s parameter and "Vec"'s template, a comp argument its names. A copy read again from any other scope then
  /// resolves through the written identities ("Scope::FindWrittenTypeSymbol"), not by its spelling.
  SPP_EXP_FUN auto RecordTypeParts(
    TypeAst const &type,
    Scope const &scope)
    -> void;

  /// "RecordTypeParts" for a comp value: every comp name in it, nested ones included ("n + 1", a pack's elements),
  /// and the types its constants are named through.
  SPP_EXP_FUN auto RecordCompParts(
    ExpressionAst const &value,
    Scope const &scope)
    -> void;

  /// The parameter each of "args" binds, in parallel: a keyword argument the parameter it names; a positional one the
  /// next parameter of its kind (type or "cmp") that no keyword argument names, in order, a variadic one taking the
  /// rest of its kind. Null for an argument naming no parameter, or past them. The one rule every naming of
  /// positional arguments uses ("NamedGnArgs", an instantiation's identity, its recorded arguments).
  SPP_EXP_FUN auto ParamsOfArgs(
    Vec<GenericArgumentAst*> const &args,
    Vec<GenericParameterAst*> const &params)
    -> Vec<GenericParameterAst*>;

  /// "BindArgs" for what a match inferred ("type_compare::GenericInferenceMap"): each parameter of "params" with a value
  /// in "inferred", bound to that value's identity read in "scope", built directly rather than through the arguments a
  /// map would make. A parameter with no value is left out.
  SPP_EXP_FUN auto BindInferred(
    type_compare::GenericInferenceMap const &inferred,
    GenericParameterGroupAst const &params,
    Scope const &scope)
    -> scopes::GenericSubst;

  /// "BindByName" for the named arguments "args", each read in
  /// "scope": what a type written in the terms of "params" is read
  /// with ("ReadType"). A parameter with no argument is left out.
  SPP_EXP_FUN auto BindArgs(
    GenericParameterGroupAst const &params,
    Vec<GenericArgumentAst*> const &args,
    Scope const &scope)
    -> scopes::GenericSubst;

  /// Bind "Self" in "subst" (parameter 0) to "self_type", read in "scope". Only where "Self" is meant to be read as
  /// what a use pinned it to (a default carried to the use); a type argument otherwise keeps "Self" as written.
  SPP_EXP_FUN auto BindSelf(
    scopes::GenericSubst &subst,
    TypeAst const &self_type,
    Scope const &scope)
    -> void;

  /// What an instantiation binds its class's parameters to, read off its identity (defaults filled), as a type written
  /// in the class's terms is read with. Empty for anything that is not a class instance.
  SPP_EXP_FUN auto InstanceBindings(
    TypeRef const &inst)
    -> scopes::GenericSubst;

  /// A type read with "sub" ("ExprSubst"): keyed where written, its bindings applied ("SubstituteTypeId"), and named
  /// from that identity where read ("TypeAstOf"). A type that does not resolve is kept as written.
  SPP_EXP_FUN auto ReadType(
    TypeAst const &written,
    scopes::ExprSubst const &sub)
    -> Shared<TypeAst>;

  /// "ReadType" for a comp value: by its identity ("SubstituteCompId", "CompAstOf") where it has one; else, holding an
  /// opaque part ("Self::mo_seq_cst", a call), as an expression ("ReadExpr"), whose types and comp names
  /// are read by identity too. A name with no identity (a local) is kept as written.
  SPP_EXP_FUN auto ReadComp(
    ExpressionAst const &written,
    scopes::ExprSubst const &sub)
    -> Shared<ExpressionAst>;

  /// A type parameter's default, written where "param" is ("written_in"), read in "read_in" with "bindings" applied
  /// ("ReadType"). Null when there is no default, or it does not resolve.
  SPP_EXP_FUN auto ReadTypeDefault(
    GenericParameterAst const &param,
    scopes::GenericSubst bindings,
    Scope const &written_in,
    Scope const &read_in)
    -> Shared<TypeAst>;

  /// "ReadTypeDefault" for a comp parameter's default, read as written ("GenericParameterAst::WrittenCompDefault":
  /// analysing it desugars its operators) ("ReadComp"). Null when there is no default.
  SPP_EXP_FUN auto ReadCompDefault(
    GenericParameterAst const &param,
    scopes::GenericSubst bindings,
    Scope const &written_in,
    Scope const &read_in)
    -> Shared<ExpressionAst>;

  /// "args" as the instantiation they name would record them: one per parameter, in parameter order, named after it -
  /// a keyword argument where given, else the next positional one ("ParamsOfArgs"), else the parameter's default
  /// ("ReadTypeDefault", "ReadCompDefault"), read where the parameters are declared ("decl_scope") with what is named
  /// so far bound. What keying an instantiation not made yet reads its arguments as ("Scope::TypeKey"). Empty when the
  /// parameters are variadic, whose packing this does not attempt, or when a default does not resolve.
  SPP_EXP_FUN auto RecordedArgsFor(
    GenericArgumentGroupAst const &args,
    GenericParameterGroupAst const &params,
    Scope const &scope,
    Scope const &decl_scope)
    -> Vec<Unique<GenericArgumentAst>>;
}
