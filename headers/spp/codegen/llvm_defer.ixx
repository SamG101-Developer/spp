module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_defer;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::codegen {
  /// Generate the deferred expressions belonging to the scope,
  /// in reverse order; the statements run last-registered-first,
  /// so a value deferred after another is released before it.
  /// Only the defer statements that a walk has actually reached
  /// are emitted.
  SPP_EXP_FUN auto EmitDeferredScope(
    Scope const &scope,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;

  /// Similar to above, but generate the deferred expression of
  /// an innermost scope, moving upwards until the boundary scope
  /// is reached. This is when we return from a loop early for
  /// example, or any nested structure. The deferred statements
  /// run at the jump.
  SPP_EXP_FUN auto EmitDeferredUnwind(
    Scope const &innermost,
    Scope const *boundary,
    bool boundary_inclusive,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
