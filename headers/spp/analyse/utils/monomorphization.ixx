module;
#include <spp/macros.hpp>

export module spp.analyse.utils.monomorphization;
import spp.analyse.scopes.type_key;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct ScopeTree);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

namespace spp::analyse::utils::monomorphization {
  // Class instances.

  /// One level of class instantiation, for as long as it
  /// lives: one being analysed ("TypeIdentifierAst::Stage7")
  /// or made from its identity ("InstantiateForScope"). An
  /// instantiation that keeps nesting ("Box[T]" inside "Box"'s
  /// own "sup", with "T" bound to "Box[T]") never ends, so past
  /// 128 levels it is reported on "at", rather than left to
  /// overflow the stack.
  SPP_EXP_CLS class InstantiationDepth {
  public:
    InstantiationDepth(TypeAst const &at);
    ~InstantiationDepth();
    InstantiationDepth(InstantiationDepth const &) = delete;
    auto operator=(InstantiationDepth const &) -> InstantiationDepth& = delete;
  };

  /// Entry point for monomorphizing a type, which in turn uses a
  /// "cls" scope. It is the instance of the "old_cls_sym"'s
  /// representing type, under the arguments of the "type_part".
  SPP_EXP_FUN auto FindOrMakeClsInstance(
    TypeIdentifierAst &type_part, Shared<TypeSymbol> const &old_cls_sym,
    TypeId args_id, bool is_tuple, ScopeManager *sm,
    meta::CompilerMetaData *meta) -> TypeSymbol*;

  // Superimposition instances.

  /// Make the instance of the "sup" block "old_sup_scope" for
  /// the class instance "new_cls_scope", bound to the arguments
  /// whose identity is "args_id", attached beside the block.
  SPP_EXP_FUN auto MakeSupInstance(
    Scope &old_sup_scope, Scope &new_cls_scope, TypeId args_id,
    ScopeManager const *sm, meta::CompilerMetaData *meta) -> Tup<Scope*, Scope*>;

  // Function instances.

  /// The prototype a call to "fn_proto" with the arguments whose
  /// identity is "args_id" ("Scope::ArgsIdOf", keyed at the call;
  /// "GenericSolver::SolvedArgsId") resolves to: the template
  /// itself when they pin nothing, otherwise its substitution,
  /// built the first time it is reached. A call is checked against
  /// it before the call knows which overload it makes, so it is
  /// not required ("FunctionPrototypeAst::RequireGnSubstitution") -
  /// nothing analyses or emits it until a call chooses it.
  /// "variadic_pack_type" is the tuple a variadic parameter's
  /// arguments form.
  SPP_EXP_FUN auto FindOrMakeFnInstance(
    FunctionPrototypeAst *fn_proto, Scope const *fn_scope, TypeId args_id,
    Shared<TypeAst> const &variadic_pack_type, ScopeManager *sm, meta::CompilerMetaData *meta)
    -> Tup<FunctionPrototypeAst*, Scope const*>;

  /// As "FindOrMakeFnInstance", for arguments already named (read
  /// off a "sup" block that binds them: a destructor, a function
  /// value), and require the instance, so it is analysed and
  /// emitted.
  SPP_EXP_FUN auto RequireFnInstance(
    FunctionPrototypeAst *fn_proto, Scope const *fn_scope, TypeId args_id,
    ScopeManager *sm, meta::CompilerMetaData *meta) -> FunctionPrototypeAst*;

  /// The instance of "fn_proto" for the arguments whose identity
  /// is "args_id", once required ("RequireFnInstance"): the
  /// template itself when they pin nothing, and null when it has
  /// not been required.
  SPP_EXP_FUN auto FindRequiredFnInstance(
    FunctionPrototypeAst *fn_proto, TypeId args_id) -> FunctionPrototypeAst*;

  // Instantiating on read.

  /// Let reading a type ("TypeRef::Of") make an instantiation not
  /// made yet ("InstantiateForScope"), made through "tree" as
  /// "stage" makes one, until "StopInstantiatingOnRead". Only the
  /// analysis stages can make one ("CompilerBoot"): one made after
  /// them would never be generated.
  SPP_EXP_FUN auto StartInstantiatingOnRead(
    ScopeTree &tree, meta::CompilerStage stage) -> void;

  /// Stop the TypeRef::Of and other type reads from creating any
  /// not-existing instantiations.
  SPP_EXP_FUN auto StopInstantiatingOnRead() -> void;

  /// Make the instantiation "id" names where "scope" reads it (already
  /// read there). A class's is made from the identity itself, its
  /// arguments already named and solved; an alias's or a variant's
  /// name is analysed, as a name written there would be. Null when
  /// reading makes nothing now ("StartInstantiatingOnRead"), or that
  /// fails.
  SPP_EXP_FUN auto InstantiateForScope(
    TypeId id, Scope const &scope) -> TypeSymbol*;

  // The drain.

  /// Record that a "fn_template" has a substitution needing
  /// processing. This is a new generically-instantiated
  /// version of a function prototype. A template already
  /// waiting is not queued again; one already drained is.
  SPP_EXP_FUN auto EnqueueInstantiation(
    FunctionPrototypeAst *fn_template) -> void;

  /// Drain the instantiation queue's pending list, taking
  /// each element from the list and analysing all the pending
  /// generic instantiations of it.
  SPP_EXP_FUN auto MonomorphiseToFixedPoint(
    ScopeManager *sm, meta::CompilerMetaData *meta) -> void;

  /// Drop everything waiting. Called between compilations,
  /// because the record outlives any one of them; prevents
  /// sharing state or memory leaks.
  SPP_EXP_FUN auto ClearInstantiations() -> void;
}
