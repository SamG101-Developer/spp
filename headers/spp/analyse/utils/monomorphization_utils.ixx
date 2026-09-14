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

  /// Create the generic substitution for a class, and register
  /// it against the base class. This adds information into the
  /// module symbol tables / scope tree etc.
  SPP_EXP_FUN auto CreateGenericClsScope(
    TypeIdentifierAst &type_part,
    Shared<TypeSymbol> const &old_cls_sym,
    Vec<Shared<Symbol>> const &external_generic_syms,
    bool is_tuple,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Scope*;

  /// Create generic substitution for a function, and register
  /// it against the base function.
  SPP_EXP_FUN auto CreateGenericFunScope(
    Scope const &old_fun_scope,
    GenericArgumentGroupAst const &generic_args,
    Vec<Shared<Symbol>> const &external_generic_syms,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Scope*;

  /// Create generic substitution for a superimposition, and
  /// register it against the internal superimposition cache.
  SPP_EXP_FUN auto CreateGenericSupScope(
    Scope &old_sup_scope,
    Scope &new_cls_scope,
    GenericArgumentGroupAst const &generic_args,
    Vec<Shared<Symbol>> const &external_generic_syms,
    ScopeManager const *sm,
    meta::CompilerMetaData *meta)
    -> Tup<Scope*, Scope*>;

  /// Clear the static queue to prevent memory leaks.
  SPP_EXP_FUN auto ClearSupScopeInstantiations() -> void;
}
