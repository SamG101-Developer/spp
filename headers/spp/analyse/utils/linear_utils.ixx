module;
#include <spp/macros.hpp>

export module spp.analyse.utils.linear_utils;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::linear_utils {
  /// Check and record what the deferred statements for this
  /// scope do when they run. A defer does nothing when written,
  /// because by definition it is being deferred to the end of
  /// the scope. So only when the scope leaves do the deferred
  /// statements run, each checked against the state this exit
  /// is reached with ("DeferStatementAst::CheckAtExit").
  SPP_EXP_FUN auto CheckDeferredForScope(
    Scope const &scope,
    Ast const &exit_point,
    StrView exit_what,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// Check all the symbols created in the scope, and ensure
  /// that they adhere to the linear type system rules - they
  /// must all have been consumed at the scope boundary, unless
  /// they are of a copyable type. Also checks for partially
  /// moved values who now can't destruct properly.
  SPP_EXP_FUN auto CheckScopeExit(
    Scope const &scope,
    Ast const &exit_point,
    StrView exit_what,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// A more expansive check that "CheckScopeExit" - this does
  /// the same thing but checks between this scope and the
  /// enclosing function's scope, allowing nested-ast "ret" or
  /// loop's "exit" to adhere to the memory system properly.
  SPP_EXP_FUN auto CheckLiveUpToFn(
    Ast const &exit_point,
    StrView exit_what,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// The loop control flow includes "skip" and "exit". The
  /// "exit" breaks out of n loops, so for all these scopes,
  /// we need to do the linear test on the symbols.
  SPP_EXP_FUN auto CheckLiveUpToLoop(
    Ast const &exit_point,
    StrView exit_what,
    std::size_t num_exits,
    bool has_skip,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// Assigning a whole new value to a symbol, or shadowing it
  /// in its own scope, discards the value it holds, and nothing
  /// destroys a value implicitly, so the old value must already
  /// have been consumed.
  SPP_EXP_FUN auto CheckOverwrite(
    VariableSymbol const &sym,
    Ast const &site,
    StrView site_what,
    ScopeManager &sm)
    -> void;
}
