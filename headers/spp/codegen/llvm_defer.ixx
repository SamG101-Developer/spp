module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_defer;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
  /**
   * Generate the deferred expressions belonging to @p scope , in reverse order - the statements run
   * last-registered-first, so a value deferred after another is released before it.
   *
   * @n
   * Nothing is emitted into a block that already has a terminator: the path that ended there ran its own deferred
   * expressions where it left, and anything added here would be unreachable.
   * @n
   * Only the statements the walk has actually reached are emitted, read from @c Scope::DeferredReached : a
   * @c defer below an exit never ran, so it has nothing to release.
   * @param[in] scope The scope being left.
   * @param[in] sm The scope manager, positioned in @p scope .
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDeferredScope(
    analyse::scopes::Scope const &scope,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;

  /**
   * Generate the deferred expressions of @p innermost and of every scope out to @p boundary , for control that leaves
   * them early. A @c ret or a loop @c exit jumps over the scope ends that would have run them, so they run at the jump
   * instead, innermost scope first - the order the scopes would have been left in had control reached their ends.
   * @param[in] innermost The scope control is leaving from.
   * @param[in] boundary The scope the walk stops at, or @c nullptr to walk to the root.
   * @param[in] boundary_inclusive Whether @p boundary 's own deferred expressions run. A @c ret leaves the function
   * scope it stops at; a loop jump stops at the scope the loop sits in, which it is not leaving.
   * @param[in] sm The scope manager, positioned in @p innermost .
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDeferredUnwind(
    analyse::scopes::Scope const &innermost,
    analyse::scopes::Scope const *boundary,
    bool boundary_inclusive,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
