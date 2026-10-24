module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_defer;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct DeferStatementAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct DeferredReached);

/// The "defer"s a codegen walk has reached in each scope, in
/// order: what leaving the scope runs ("EmitScopeExit"), so an
/// exit part-way through a scope runs only those above it.
SPP_EXP_CLS struct spp::codegen::DeferredReached {
  Map<Scope const*, Vec<asts::DeferStatementAst*>> InScope;
};

namespace spp::codegen {
  /// The walk enters "scope", so nothing in it is reached yet. A
  /// scope can be walked more than once in a compile: a deferred
  /// block is generated at each exit that runs it, and a generic
  /// template and its instantiations share their scopes.
  SPP_EXP_FUN auto EnterDeferScope(
    Scope const &scope,
    LlvmCtx *ctx)
    -> void;

  /// The walk has passed "defer" in "scope", so the exits below it
  /// run it. Recorded once, however often the walk passes it.
  SPP_EXP_FUN auto ReachDefer(
    Scope const &scope,
    asts::DeferStatementAst &defer,
    LlvmCtx *ctx)
    -> void;

  /// Leave every scope from the current one out to "into", the
  /// scope control lands in, running what each deferred: the
  /// innermost first, and within a scope the last registered
  /// first, so a value deferred after another is released before
  /// it. A block's end lands in its parent, a "ret" in its
  /// function's parent, an "exit" in the loop's parent and a
  /// "skip" in the loop's own scope. Only the "defer"s a walk
  /// has reached run, and none on a path that has already left.
  SPP_EXP_FUN auto EmitScopeExit(
    Scope const *into,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
