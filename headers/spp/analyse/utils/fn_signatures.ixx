module;
#include <spp/macros.hpp>

export module spp.analyse.utils.fn_signatures;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct FunctionPrototypeAst);

/// Comparing function signatures: whether a new overload conflicts
/// with one already declared, and whether a method is a genuine
/// override of a super type's.
namespace spp::analyse::utils::fn_signatures {
  /// Check whether the new function prototype conflicts with
  /// functions of the same name but different signatures with
  /// the same owner. Two "fun f(&self) -> Void" is ambiguous.
  /// Several semantic checks in place to detect ambiguities.
  /// The conflicting prototype and the scope it is declared in
  /// (another module's, for a "sup" block written elsewhere);
  /// both null for no conflict.
  SPP_EXP_FUN auto CheckForConflictingOverload(
    Scope const &this_scope, Scope const *target_scope,
    FunctionPrototypeAst const &new_fn, ScopeManager &sm,
    meta::CompilerMetaData *meta) -> Pair<FunctionPrototypeAst*, Scope const*>;

  /// The core of the override checker, checking whether "fn_a"
  /// (the override or implementation) has "fn_b"'s signature: the
  /// same parameters, and a return type that may narrow "fn_b"'s.
  /// Also used by overload resolution, where a derived method with
  /// a base method's signature hides it.
  SPP_EXP_FUN auto SameSignature(
    FunctionPrototypeAst const &fn_a, Scope const &scope_a,
    FunctionPrototypeAst const &fn_b, Scope const &scope_b) -> bool;

  /// Check whether the new function prototype is a genuine
  /// override of a method on a super type: the method on
  /// "target_scope" with the same signature, if any. This is used
  /// to ensure function overriding is a genuine match.
  SPP_EXP_FUN auto CheckForConflictingOverride(
    Scope const &this_scope, Scope const *target_scope, FunctionPrototypeAst const &new_fn,
    ScopeManager &sm, meta::CompilerMetaData *meta)
    -> FunctionPrototypeAst*;
}
