module;
#include <spp/macros.hpp>

export module spp.analyse.utils.linear_utils;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::analyse::utils::linear_utils {
  /**
   * Record what this scope's deferred expressions take when they run. A @c defer does not consume anything where it is
   * written - the value has to stay usable for the rest of the scope - so leaving the scope is what consumes it, and
   * this is where that is written down, immediately before the scope is held to the linear rule.
   * @param scope The scope being left.
   * @param exit_point The ast blamed for the move, which is the @c defer itself.
   * @param sm The scope manager, used to resolve the recorded names against @p scope .
   */
  SPP_EXP_FUN auto CheckDeferredForScope(
    scopes::Scope const &scope,
    asts::Ast const &exit_point,
    StrView exit_what,
    scopes::ScopeManager &sm)
    -> void;

  /**
   * Raise @c SppLinearValueNotConsumedError for every symbol declared in this scope that is still live. Called at the
   * exit of a function body scope and of every inner scope inside one.
   * @param scope The scope being left.
   * @param exit_point The ast to report the error against - the closing brace, or the statement that leaves early.
   * @param exit_what How to describe the exit point in the message, for example "Scope ends here".
   * @param sm The scope manager, positioned in @p scope .
   * @param meta Associated metadata.
   */
  SPP_EXP_FUN auto CheckScopeExit(
    scopes::Scope const &scope,
    asts::Ast const &exit_point,
    StrView exit_what,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData const *meta)
    -> void;

  /**
   * Raise @c SppLinearValueNotConsumedError for every live symbol between the current scope and the enclosing
   * function scope inclusive. A @c ret or a loop @c exit leaves all of those scopes at once, so a value held in any of
   * them is abandoned rather than used, which the per-scope check would never see: control never reaches the closing
   * brace those scopes would have been checked at.
   * @param exit_point The statement leaving the scopes.
   * @param exit_what How to describe the exit point in the message.
   * @param sm The scope manager, positioned at the statement.
   * @param meta Associated metadata, read for @c EnclosingFunctionScope .
   */
  SPP_EXP_FUN auto CheckLiveUpToFunction(
    asts::Ast const &exit_point,
    StrView exit_what,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData const *meta)
    -> void;

  /**
   * The @c exit and @c skip counterpart of @c CheckLiveUpToFunction . An @c exit leaves @p num_exits loops, so every
   * scope up to and including the @p num_exits th enclosing loop scope is abandoned; a trailing @c skip then leaves
   * the body of the loop after those, but not that loop's own scope, which the next iteration re-enters.
   * @param exit_point The statement leaving the scopes.
   * @param exit_what How to describe the exit point in the message.
   * @param num_exits How many loops the statement exits, which is zero for a bare @c skip .
   * @param has_skip Whether the statement ends in a @c skip .
   * @param sm The scope manager, positioned at the statement.
   * @param meta Associated metadata.
   */
  SPP_EXP_FUN auto CheckLiveUpToLoop(
    asts::Ast const &exit_point,
    StrView exit_what,
    std::size_t num_exits,
    bool has_skip,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData const *meta)
    -> void;
}
