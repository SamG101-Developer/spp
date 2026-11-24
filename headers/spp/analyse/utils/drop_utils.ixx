module;
#include <spp/macros.hpp>

export module spp.analyse.utils.drop_utils;
import spp.utils.interner;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::utils::memory_state, struct MemoryState);
use(spp::analyse::utils::regions, struct PlaceStep);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::drop_utils {
  /// A value still owned where its lifetime ends, for code
  /// generation to drop there: the symbol's name, and the paths
  /// of the parts already moved out of it (field names, outermost
  /// first), none for a whole value. Found again by name, so an
  /// instance finds its own copy, and by where the name is
  /// declared ("DeclaredAt"), as a scope can own two of one name
  /// (one displaced by the other). None for a value nothing
  /// declares (a codegen-built "gen"'s "self").
  SPP_EXP_CLS struct OwnedValue {
    Shared<IdentifierAst> Name;
    Vec<Vec<spp::utils::InternedId>> Moved;
    Opt<std::size_t> DeclaredAt;

    /// "self" at the end of its own type's "drop": only its fields
    /// are dropped, as running the destructor again would recurse.
    bool Structural = false;
  };

  /// What a way out of several scopes drops: one entry per scope
  /// left, innermost first, each newest first ("OwnedIn").
  SPP_EXP_CLS using OwnedScopes = Vec<Vec<OwnedValue>>;

  /// What "state" (one path's state of "sym", declared in
  /// "owner_scope") still owns of it, for that path to drop: none
  /// for a value it does not hold, or holds on only some of the
  /// paths that met before it (they dropped it then). Its type's
  /// destructor is instantiated, and one missing a part its
  /// destructor needs is an error (E106).
  SPP_EXP_FUN auto OwnedOnPath(
    VariableSymbol const &sym, MemoryState const &state, Scope const &owner_scope,
    Ast const &at, ScopeManager &sm, CompilerMetaData *meta) -> Opt<OwnedValue>;

  /// The owners of the scopes around "scope" (from its parent out to
  /// the function's, "EnclosingFnScope"), each with the scope declaring
  /// it: what a join inside "scope" can find held on only some of its
  /// paths. "scope"'s own are left out: a "case"'s or loop's holds "is"
  /// bindings that exist only where they matched.
  SPP_EXP_FUN auto OwnersAround(
    Scope const &scope, CompilerMetaData const *meta) -> Map<VariableSymbol const*, Scope const*>;

  /// Where paths meet, what the path whose state of "sym" is "path"
  /// still holds of it, for that path to drop at its end: none unless
  /// "sym" is one of "owners" and the paths disagree about it
  /// ("joined" holds it on only some). "at" is where, for an error.
  SPP_EXP_FUN auto OwnedOnJoinPath(
    VariableSymbol const &sym, MemoryState const &joined, MemoryState const &path,
    Map<VariableSymbol const*, Scope const*> const &owners, Ast const &at,
    ScopeManager &sm, CompilerMetaData *meta) -> Opt<OwnedValue>;

  /// The part at "steps" taken out of a value of type "root" (by "move")
  /// leaves a hole in every value the path runs through, the root
  /// included: E106 if one of them has its own "drop", which could not
  /// run on what is left. The part the path ends on hands its own
  /// "drop" to whoever took it. "exit_point" is where the rest ends.
  SPP_EXP_FUN auto RaiseIfPathStrandsDestructor(
    TypeRef const &root, Vec<regions::PlaceStep> const &steps, Ast const &move,
    Ast const &exit_point, ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// Sort a path's drops newest first, by where each is declared.
  SPP_EXP_FUN auto SortNewestFirst(
    Vec<OwnedValue> &owned) -> void;


  /// The end of "scope" at "end": what its owners
  /// ("Scope::GetOwners") still hold there, newest first, for code
  /// generation to drop ("OwnedOnPath"). One held on only some of
  /// the paths that met is not listed: the paths still holding it
  /// dropped it before they met.
  SPP_EXP_FUN auto EndRegion(
    Scope const &scope, Ast const &end,
    ScopeManager &sm, CompilerMetaData *meta) -> Vec<OwnedValue>;

  /// The loans what "scope" owns hold end with it.
  SPP_EXP_FUN auto ReleaseOwnedLoans(
    Scope const &scope) -> void;

  /// What a statement's end ends ("EndStatementBindings"): its "is"
  /// bindings ("ExpiresWithStatement"), the "$" temporaries a
  /// lowering made inside it ("VariableKind::Temporary"), or both.
  SPP_EXP_CLS enum class StatementEnds { kIsBindings, kTemporaries, kBoth };

  /// The end of a statement, for the bindings of "scope" among
  /// "bindings" from "from" on that "ends" names: nothing can name
  /// one after it, so what they still own is dropped there. A
  /// statement that is itself the "let" of a temporary
  /// ("own_let", a lowering's "let $x = .." read by the statements
  /// after it) does not end that one.
  SPP_EXP_FUN auto EndStatementBindings(
    Scope const &scope, Vec<VariableSymbol*> const &bindings, std::size_t from, Ast const &end,
    StatementEnds ends, ScopeManager &sm, CompilerMetaData *meta,
    IdentifierAst const *own_let = nullptr) -> Vec<OwnedValue>;

  /// A loop's iteration starts afresh for the bindings its own
  /// scope declares (its condition's "is" bindings): their states
  /// from an earlier iteration are forgotten, so each reads as on
  /// the first.
  SPP_EXP_FUN auto ForgetIterationBindings(
    Scope const &loop_scope) -> void;

  /// Leaving the function early at "exit_point" (a "ret" or "?"):
  /// every scope out to the function's ends there ("EndRegion"), on
  /// a copy of the state, which the walk goes on past. What each
  /// still owns, innermost first.
  SPP_EXP_FUN auto OwnedUpToFn(
    Ast const &exit_point,
    ScopeManager &sm, CompilerMetaData *meta) -> OwnedScopes;

  /// "OwnedUpToFn" for an "exit" out of "num_exits" loops: every
  /// scope out to the last loop left ends there. With a "skip"
  /// after the "exit"s ("has_skip"), the loop it continues stays,
  /// but its iteration ends: what an "is" in its condition bound
  /// is dropped, into "iteration".
  SPP_EXP_FUN auto OwnedUpToLoop(
    Ast const &exit_point, std::size_t num_exits,
    bool has_skip, ScopeManager &sm, CompilerMetaData *meta,
    Vec<OwnedValue> &iteration) -> OwnedScopes;

  /// What a coroutine abandoned at "suspend_point" (a "gen") still
  /// owns there, for its frame's destruction to drop: each scope
  /// from the current one out to the coroutine's own. Every borrow
  /// has ended there.
  SPP_EXP_FUN auto OwnedAtSuspend(
    Ast const &suspend_point, ScopeManager &sm,
    CompilerMetaData *meta) -> OwnedScopes;

  /// Writing a new value over "target" (a part of "sym": "p.a",
  /// "self.a" through "&mut self", or what "x@" points at) drops
  /// the value there first, if one is owned: the parts already
  /// moved out of it, or none for one not owned (a "Copy" one, one
  /// taken, or held on only some paths).
  SPP_EXP_FUN auto OwnedBeforeWrite(
    VariableSymbol const *sym, ExpressionAst &target, ScopeManager &sm,
    CompilerMetaData *meta) -> Opt<Vec<Vec<spp::utils::InternedId>>>;

  /// Find the "drop" method superimposed over the type via
  /// the "std::ops::drop::Drop" type (either directly or via
  /// a base class being extended). Nullptr for when a drop
  /// method isn't provided (std::mem::ops::drop fallback).
  SPP_EXP_FUN auto FindDropOverload(
    TypeSymbol const &type_sym, ScopeManager &sm,
    CompilerMetaData *meta) -> FunctionPrototypeAst*;

  /// Whether the call to "proto" ends the value "arg" gives it,
  /// so nothing can use it, or what it borrows, afterwards:
  /// "std::mem::ops::drop", or the receiver ("is_receiver") of the
  /// value's own "Drop::drop".
  SPP_EXP_FUN auto EndsArgument(
    FunctionPrototypeAst const &proto, FunctionCallArgumentAst const &arg, bool is_receiver,
    ScopeManager &sm, CompilerMetaData *meta) -> bool;

  /// Whether a type needs dropping or not - if is has a drop
  /// method of its own, or contains a fields that does. Some
  /// primitive type wrappers - integers, bools, structs of
  /// them - destroy to nothing, so have no drop code. Borrows
  /// are not dropped through either, only owned values.
  SPP_EXP_FUN auto NeedsDrop(
    TypeRef const &type, ScopeManager &sm,
    CompilerMetaData *meta) -> bool;

  /// When a value is being dropped, ensure that the drop
  /// method and all of its fields' drop methods are available
  /// (have been generated). The standard tree walk may have not
  /// reached it yet when needed. Banning recursive types in
  /// semantic analysis means we will never get a cycle here
  /// either.
  SPP_EXP_FUN auto EnsureDropInstantiated(
    TypeRef const &type, ScopeManager &sm,
    CompilerMetaData *meta) -> void;

  /// Whether "expr" makes a temporary: a value that names no place
  /// ("mk()", "Str::from(..)", "(a, b)"), owned rather than borrowed,
  /// and not "Copy". Nothing holds it past the full expression making
  /// it, so one that expression does not move on is dropped as its
  /// statement ends.
  SPP_EXP_FUN auto IsTemporary(
    ExpressionAst &expr, ScopeManager &sm,
    CompilerMetaData *meta) -> bool;
}
