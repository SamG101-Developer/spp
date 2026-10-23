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
  /// Leave every scope from the current one out to "into", the
  /// scope control lands in, running what each deferred: the
  /// innermost first, and within a scope the last registered
  /// first, so a value deferred after another is released before
  /// it. A block's end lands in its parent, a "ret" in its
  /// function's parent, an "exit" or a "skip" in the loop's own
  /// scope. Only the "defer"s a walk has reached run, and none
  /// on a path that has already left.
  SPP_EXP_FUN auto EmitScopeExit(
    Scope const *into,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
