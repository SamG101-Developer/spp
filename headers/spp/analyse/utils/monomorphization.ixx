module;
#include <spp/macros.hpp>

export module spp.analyse.utils.monomorphization;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

namespace spp::analyse::utils::monomorphization {
  /// Drain the instantiation queue's pending list, taking
  /// each element from the list and analysing all the pending
  /// generic instantiations of it.
  SPP_EXP_FUN auto MonomorphiseToFixedPoint(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void;

  /// Rewrite each argument that names a bound generic as what it
  /// is bound to, read from "scope": a type argument as the bound
  /// type's qualified name, a comp argument as its value (or what
  /// it folds to). A function instantiation's arguments are
  /// recorded this way.
  auto CanonicaliseGenericArgs(
    GenericArgumentGroupAst &args,
    Scope const &scope)
    -> void;

  /// Create the generic substitution for a class, and register
  /// it against the base class. This adds information into the
  /// module symbol tables / scope tree etc.
  SPP_EXP_FUN auto CreateGenericClsScope(
    TypeIdentifierAst &type_part,
    Shared<TypeSymbol> const &old_cls_sym,
    bool is_tuple,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Scope*;

  /// Create generic substitution for a function, and register
  /// it against the base function.
  auto CreateGenericFunScope(
    Scope const &old_fun_scope,
    GenericArgumentGroupAst const &generic_args,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Scope*;

  /// Create generic substitution for a superimposition, and
  /// register it against the internal superimposition cache.
  SPP_EXP_FUN auto CreateGenericSupScope(
    Scope &old_sup_scope,
    Scope &new_cls_scope,
    GenericArgumentGroupAst const &generic_args,
    ScopeManager const *sm,
    meta::CompilerMetaData *meta)
    -> Tup<Scope*, Scope*>;

  /// Make the instantiation an open one stands for where "scope"
  /// reads it, when re-keying it through that scope's bindings names
  /// one not made yet ("Scope::OnInstantiationMissing"): its qualified
  /// name is analysed there, as a name written there would be. Null
  /// if that analysis fails, or it is already being made.
  SPP_EXP_FUN auto InstantiateForScope(
    TypeSymbol &open_instance,
    Scope const &scope,
    Shared<Scope> const &global_scope,
    meta::CompilerMetaData *meta)
    -> TypeSymbol*;

  /// The prototype a call to "fn_proto" with "combined_generics"
  /// resolves to: the template itself when the arguments pin
  /// nothing, otherwise its substitution, created and queued for
  /// analysis the first time it is reached. "variadic_pack_type"
  /// is the tuple a variadic parameter's arguments form.
  SPP_EXP_FUN auto PotentiallyGenerateGenericSubstitutedPrototype(
    FunctionPrototypeAst *fn_proto,
    Scope const *fn_scope,
    GenericArgumentGroupAst &combined_generics,
    Shared<TypeAst> const &variadic_pack_type,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Tup<FunctionPrototypeAst*, Scope const*>;

  /// Whether "scope" is inside a template: it, or a scope around
  /// it, declares a generic parameter with no binding (an unbound
  /// type or comp parameter). Nothing written there that depends
  /// on one has a value yet; an instantiation's scopes bind them.
  SPP_EXP_FUN auto IsInTemplate(Scope const &scope) -> bool;

  /// Instantiate a generic substitution of a function, creating
  /// the new body etc for analysis. Typically called from the
  /// function_values wrapper.
  SPP_EXP_FUN auto InstantiateOverload(
    FunctionPrototypeAst *fn_proto,
    Scope const *fn_scope,
    GenericArgumentGroupAst &generic_args,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> FunctionPrototypeAst*;

  /// Find an instantiated overload, given a function prototype
  /// and a generic argument group. Typically called from the
  /// function_values wrapper.
  SPP_EXP_FUN auto FindInstantiatedOverload(
    FunctionPrototypeAst *fn_proto,
    GenericArgumentGroupAst &generic_args,
    ScopeManager const *sm)
    -> FunctionPrototypeAst*;

  // The record of which function templates have had a generic
  // substitution registered against them that hasn't been
  // processed yet. Instantiations are discovered lazily, by
  // analysing calls to generic prototypes, such as "1 + 2"
  // creating "SizedInteger[32, true]::add", whose body in turn
  // creates "intrinsics::add[T=S32]". They are drained to a
  // fixed point, so the set is complete before anything
  // downstream depends on it.

  /// Record that a "fn_template" has a substitution needing
  /// processing. This is a new generically-instantiated
  /// version of a function prototype. If a template is
  /// re-queued, it is a no-op.
  SPP_EXP_FUN auto EnqueueInstantiation(FunctionPrototypeAst *fn_template) -> void;

  /// Take the next template waiting to be processed, in the
  /// order they were recorded. This ordering, whilst not
  /// required for the overall processing, allows for errors
  /// in one instantiation to be reported against the genuine
  /// call for it rather than an unrelated later one.
  auto PopInstantiation() -> FunctionPrototypeAst*;

  /// Drop everything waiting. Called between compilations,
  /// because the record outlives any one of them; prevents
  /// sharing state or memory leaks.
  SPP_EXP_FUN auto ClearInstantiations() -> void;

}
