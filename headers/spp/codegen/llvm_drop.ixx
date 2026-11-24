module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_drop;
import spp.analyse.utils.drop_utils;
import spp.codegen.llvm_ctx;
import spp.utils.interner;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct PendingTemporaries);
use(spp::codegen, struct PendingTemporary);

/// A temporary a statement made and did not move on (borrowed, a part read
/// off it, a "case" over it): dropped where the statement ends, or where an
/// exit leaves it first. Less the parts already moved out of it.
SPP_EXP_CLS struct spp::codegen::PendingTemporary {
  llvm::Value *Ptr = nullptr;
  Shared<TypeRef> Type;
  Vec<Vec<spp::utils::InternedId>> Moved;
};

/// The temporaries a codegen walk holds, still to drop, oldest first
/// ("PushTemporary").
SPP_EXP_CLS struct spp::codegen::PendingTemporaries {
  Vec<PendingTemporary> Held;
};

namespace spp::codegen {
  /// Emit the destruction of the value at a "ptr", which is the pointer to
  /// storage holding a live value of "type". A borrow owns nothing, so destroys
  /// nothing. A type superimposing Drop has its own destructor to run first,
  /// and then every field is dropped in reverse order, recursively, running
  /// their drop functions or fields recursively, etc. A value some of whose
  /// parts were moved out (only an abandoned generator frame holds one) passes
  /// their paths in "moved", field names outermost first, and only what is left
  /// is destroyed, part by part. At each place in "taken_apart" (paths
  /// the same way, the empty path being the value itself) the type's own
  /// destructor is not run, only its fields dropped: what a destructure
  /// took apart, and "self" at the end of that destructor.
  SPP_EXP_FUN auto EmitDrop(
    TypeRef const &type, llvm::Value *ptr, ScopeManager *sm,
    CompilerMetaData *meta, LlvmCtx *ctx,
    Vec<Vec<utils::InternedId>> const &moved = {},
    Vec<Vec<utils::InternedId>> const &taken_apart = {}) -> void;

  /// Leave every scope from the current one out to "into", the scope control
  /// lands in, each dropping what it still owns: "owned" holds one entry per
  /// scope, innermost first, as analysis recorded them for this exit; a scope
  /// with no entry drops nothing. A block's end lands in its parent, a "ret" in
  /// its function's parent, an "exit" in the loop's parent and a "skip" in the
  /// loop's own scope. Nothing is emitted on a path that has already left.
  SPP_EXP_FUN auto EmitScopeExit(
    Scope const *into, ScopeManager *sm, CompilerMetaData *meta,
    LlvmCtx *ctx, analyse::utils::drop_utils::OwnedScopes const &owned = {}) -> void;

  /// Drop each value "scope" still owns ("owned", newest first, as analysis
  /// lists them), into the current block, unless the path has already left. A
  /// value partly moved out loses only what is left of it. "at" is where, for
  /// an internal error (none: the scope has no ast).
  SPP_EXP_FUN auto EmitOwnedDrops(
    Scope const &scope, Vec<analyse::utils::drop_utils::OwnedValue> const &owned,
    asts::Ast const *at, ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> void;

  /// Drop each value "owned" lists, into the current block, unless the path
  /// has already left: values of the scopes around the current one (a join's
  /// drops), each found by searching out from it. "at" is where, for an
  /// internal error.
  SPP_EXP_FUN auto EmitOuterDrops(
    Vec<analyse::utils::drop_utils::OwnedValue> const &owned, asts::Ast const &at,
    ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> void;

  /// Hold the temporary at "ptr" (of "type", less the parts "moved") until
  /// the statement making it ends ("EmitTemporaryDrops").
  SPP_EXP_FUN auto PushTemporary(
    llvm::Value *ptr, TypeRef const &type, Vec<Vec<spp::utils::InternedId>> moved,
    LlvmCtx *ctx) -> void;

  /// How many temporaries are held now: a statement, a loop or a function
  /// body notes it on entry, and drops back to it.
  SPP_EXP_FUN auto TemporaryDepth(
    LlvmCtx const *ctx) -> std::size_t;

  /// Drop the temporaries held from "from" on, newest first, into the current
  /// block, unless the path has already left. They stay held: an early exit
  /// drops them on its own edge, and the statement still ends normally.
  SPP_EXP_FUN auto EmitTemporaryDrops(
    std::size_t from, ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> void;

  /// Stop holding the temporaries from "to" on (their statement ended).
  SPP_EXP_FUN auto PopTemporaries(
    std::size_t to, LlvmCtx *ctx) -> void;

  /// Destroy the coroutine's frame at a suspension, into the current block:
  /// drop the statement's temporaries, then leave every scope out to the
  /// coroutine's own, each dropping what it still owns ("owned", one entry per
  /// scope, innermost first, as "OwnedAtSuspend" records them), then go on to
  /// the frame's cleanup. "at" is the suspension, for an internal error.
  SPP_EXP_FUN auto EmitSuspendDestroy(
    analyse::utils::drop_utils::OwnedScopes const &owned, asts::Ast const &at,
    ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> void;
}
