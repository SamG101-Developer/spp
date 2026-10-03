module;
#include <spp/macros.hpp>

export module spp.analyse.utils.monomorphization;
import spp.analyse.scopes.instance_key;
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
  /// One level of class instantiation, for as long as it lives: one being analysed ("TypeIdentifierAst::Stage7") or
  /// made from its identity ("InstantiateForScope"). An instantiation that keeps nesting ("Box[T]" inside "Box"'s own
  /// "sup", with "T" bound to "Box[T]") never ends, so past 128 levels it is reported on "at", rather than left to
  /// overflow the stack.
  SPP_EXP_CLS class InstantiationDepth {
  public:
    InstantiationDepth(TypeAst const &at, ScopeManager const &sm);
    ~InstantiationDepth();
    InstantiationDepth(InstantiationDepth const &) = delete;
    auto operator=(InstantiationDepth const &) -> InstantiationDepth& = delete;
  };

  /// Drain the instantiation queue's pending list, taking
  /// each element from the list and analysing all the pending
  /// generic instantiations of it.
  SPP_EXP_FUN auto MonomorphiseToFixedPoint(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void;

  /// Create the generic substitution for a class, and register
  /// it against the base class. This adds information into the
  /// module symbol tables / scope tree etc. "identity" is the one
  /// the instantiation is filed under ("Scope::InstanceIdOf" of
  /// "type_part"'s arguments, read where it is written).
  SPP_EXP_FUN auto CreateGnClsScope(
    TypeIdentifierAst &type_part,
    Shared<TypeSymbol> const &old_cls_sym,
    scopes::TypeId id,
    bool is_tuple,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Scope*;

  /// Create the scope of a generic substitution of a function,
  /// its bindings registered; the caller files it against the
  /// base function.
  auto CreateGnFnScope(
    Scope const &old_fun_scope,
    GenericArgumentGroupAst const &generic_args,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Unique<Scope>;

  /// Create generic substitution for a superimposition, and
  /// register it against the internal superimposition cache.
  SPP_EXP_FUN auto CreateGnSupScope(
    Scope &old_sup_scope,
    Scope &new_cls_scope,
    GenericArgumentGroupAst const &generic_args,
    ScopeManager const *sm,
    meta::CompilerMetaData *meta)
    -> Tup<Scope*, Scope*>;

  /// Let reading a type ("TypeRef::Of") make an instantiation not made
  /// yet ("InstantiateForScope"), made through "global_scope" as
  /// "stage" makes one, until "StopInstantiatingOnRead". Only the
  /// analysis stages can make one ("CompilerBoot"): one made after
  /// them would never be generated.
  SPP_EXP_FUN auto StartInstantiatingOnRead(Shared<Scope> global_scope, meta::CompilerStage stage) -> void;
  SPP_EXP_FUN auto StopInstantiatingOnRead() -> void;

  /// Make the instantiation "id" names where "scope" reads it (already
  /// read there). A class's is made from the identity itself, its
  /// arguments already named and solved; an alias's or a variant's
  /// name is analysed, as a name written there would be. Null when
  /// reading makes nothing now ("StartInstantiatingOnRead"), or that
  /// fails.
  SPP_EXP_FUN auto InstantiateForScope(scopes::TypeId id, Scope const &scope) -> TypeSymbol*;

  /// The prototype a call to "fn_proto" with "combined_generics"
  /// resolves to: the template itself when the arguments pin
  /// nothing, otherwise its substitution, built the first time it
  /// is reached. A call is checked against it before the call
  /// knows which overload it makes, so it is not required
  /// ("FunctionPrototypeAst::RequireGnSubstitution") - nothing analyses or emits it until
  /// a call chooses it. "variadic_pack_type" is the tuple a
  /// variadic parameter's arguments form.
  SPP_EXP_FUN auto PotentiallyGenerateGnSubstitutedPrototype(
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
  /// the new body etc for analysis, and require it. Typically
  /// called from the fn_values wrapper.
  SPP_EXP_FUN auto InstantiateOverload(
    FunctionPrototypeAst *fn_proto,
    Scope const *fn_scope,
    GenericArgumentGroupAst &generic_args,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> FunctionPrototypeAst*;

  /// Find an instantiated overload, given a function prototype
  /// and a generic argument group. Typically called from the
  /// fn_values wrapper.
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
