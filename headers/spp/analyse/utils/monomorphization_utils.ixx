module;
#include <spp/macros.hpp>

export module spp.analyse.utils.monomorphization_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct Symbol);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::codegen, struct LlvmCtx);

namespace spp::analyse::utils::monomorphization_utils {
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
  SPP_EXP_FUN auto CanonicaliseGenericArgs(
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
  SPP_EXP_FUN auto CreateGenericFunScope(
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
}
