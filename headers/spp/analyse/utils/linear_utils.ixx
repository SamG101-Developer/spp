module;
#include <spp/macros.hpp>

export module spp.analyse.utils.linear_utils;
import spp.utils.types;
import std;

use(spp::asts, struct Ast);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);

namespace spp::analyse::utils::linear_utils {
  /// The first part of a value that a destructure has not
  /// accounted for, and would otherwise silently drop. Copyable
  /// parts are ignored, following usual memory rules, and a
  /// value who has all fields copyable will never provide a
  /// response here.
  SPP_EXP_FUN auto FirstUnaccountedPart(
    VariableSymbol const &sym,
    Vec<IdentifierAst*> const &region,
    ScopeManager const &sm)
    -> Str;

  /// Record what the deferred statements for this scope take
  /// when they run. A defer doesn't consume anything when
  /// written, because by definition it is being deferred to
  /// the end of the scope. So only when the scope leaves do
  /// the deferred statements run, followed by linear memory
  /// system checks.
  SPP_EXP_FUN auto CheckDeferredForScope(
    Scope const &scope,
    Ast const &exit_point,
    StrView exit_what,
    ScopeManager &sm)
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
  SPP_EXP_FUN auto CheckLiveUpToFunction(
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
}
