module;
#include <spp/macros.hpp>

export module spp.analyse.utils.linear_utils;
import spp.utils.interner;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::linear_utils {
  /// A value a coroutine still owns at a suspension: the symbol's
  /// name, and the paths of the parts already moved out of it
  /// (field names, outermost first), none for a whole value.
  SPP_EXP_CLS struct OwnedValue {
    Shared<IdentifierAst> Name;
    Vec<Vec<spp::utils::InternedId>> Moved;
  };

  /// Check and record what the deferred statements for this
  /// scope do when they run. A defer does nothing when written,
  /// because by definition it is being deferred to the end of
  /// the scope. So only when the scope leaves do the deferred
  /// statements run, each checked against the state this exit
  /// is reached with ("DeferStatementAst::CheckAtExit").
  SPP_EXP_FUN auto CheckDeferredForScope(
    Scope const &scope, Ast const &exit_point, StrView exit_what,
    ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// Check all the symbols created in the scope, and ensure
  /// that they adhere to the linear type system rules - they
  /// must all have been consumed at the scope boundary, unless
  /// they are of a copyable type. Also checks for partially
  /// moved values who now can't destruct properly.
  SPP_EXP_FUN auto CheckScopeExit(
    Scope const &scope, Ast const &exit_point, StrView exit_what,
    ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// A more expansive check that "CheckScopeExit" - this does
  /// the same thing but checks between this scope and the
  /// enclosing function's scope, allowing nested-ast "ret" or
  /// loop's "exit" to adhere to the memory system properly.
  SPP_EXP_FUN auto CheckLiveUpToFn(
    Ast const &exit_point, StrView exit_what,
    ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// What a coroutine abandoned at "suspend_point" (a "gen") still
  /// owns there, for its frame's destruction to drop: for each
  /// scope from the current one out to the coroutine's own, the
  /// symbols still holding a value nothing consumed, newest first,
  /// once the "defer"s that destroying it there runs have taken
  /// theirs (checked as at any exit). One partly moved is listed
  /// with what was moved out of it, so only the rest is dropped,
  /// and must not be missing a part of a value with its own
  /// "drop" (E106, as at a scope's end); one consumed on only some paths cannot reach a valid end
  /// anyway. Every borrow has ended there: the frame's holders go
  /// with it, and the caller gave up whatever was lent out to it
  /// before it could destroy the frame.
  SPP_EXP_FUN auto OwnedAtSuspend(
    Ast const &suspend_point, ScopeManager &sm,
    CompilerMetaData *meta) -> Vec<Vec<OwnedValue>>;

  /// The loop control flow includes "skip" and "exit". The
  /// "exit" breaks out of n loops, so for all these scopes,
  /// we need to do the linear test on the symbols.
  SPP_EXP_FUN auto CheckLiveUpToLoop(
    Ast const &exit_point, StrView exit_what, std::size_t num_exits,
    bool has_skip, ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// Assigning a whole new value to a symbol, or shadowing it
  /// in its own scope, discards the value it holds, and nothing
  /// destroys a value implicitly, so the old value must already
  /// have been consumed.
  SPP_EXP_FUN auto CheckOverwrite(
    VariableSymbol const &sym, Ast const &site,
    StrView site_what, ScopeManager &sm) -> void;

  /// "CheckOverwrite" for writing one part of a value
  /// ("p.a = .."): the part written must already have been taken
  /// off it, unless it is "Copy". Through a borrow ("self.a = .."
  /// with "&mut self", or "x@ = ..") it cannot be taken first, so a
  /// non-"Copy" part is never written over; it is exchanged.
  SPP_EXP_FUN auto CheckOverwritePart(
    VariableSymbol const &sym, ExpressionAst &part, Ast const &site,
    StrView site_what, ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// A write over "part", reached through a borrow ("self.a = .."
  /// with "&mut self", or "x@ = .."): a non-"Copy" value there can
  /// never be taken first, so it is exchanged instead, never written
  /// over.
  SPP_EXP_FUN auto CheckOverwriteThroughBorrow(
    ExpressionAst &part, ScopeManager &sm,
    CompilerMetaData *meta) -> void;

  /// Whether "expr" makes a linear temporary: a value that names
  /// no place ("mk()", "Str::from(..)", "(a, b)"), owned rather than
  /// borrowed, and not "Copy". Such a value has nothing to hold it
  /// past the full expression making it, so that expression has to
  /// consume it (move it on: return, bind, pass by value).
  SPP_EXP_FUN auto IsLinearTemporary(
    ExpressionAst &expr, ScopeManager &sm,
    CompilerMetaData *meta) -> bool;

  /// A linear temporary used without being consumed ("site" is the
  /// use: a borrow of it, a read off it, a "case" over it), which
  /// would drop it on the floor.
  SPP_EXP_FUN auto RaiseTemporaryNotConsumed(
    ExpressionAst &temp, Ast const &site, StrView site_what,
    ScopeManager &sm, CompilerMetaData *meta) -> void;
}
