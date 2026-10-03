module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);

namespace spp::analyse::utils::mem_utils {
  /// What "ValidateSymbolMemory" checks and records for a use: everything by default.
  SPP_EXP_CLS struct SymbolMemoryOptions {
    /// Raise on a use of a value that has been moved.
    bool CheckMove = true;

    /// Raise on a use of a value that has been partially moved.
    bool CheckPartialMove = true;

    /// Raise on a move out of a borrowed context (an attribute of a borrow).
    bool CheckMoveFromBorrowedCtx = true;

    /// Record the use as a move.
    bool MarkMoves = true;

    /// Raise on a move of a borrow out of the scope it is valid in.
    bool CheckEscapingBorrowMove = true;

    /// Whether the use writes the place rather than reading it (an assignment's left side).
    bool IsPlaceWritten = false;
  };

  /// Gigantic analysis set on the memory status of a value,
  /// based on how we are using it ("options"), and the current
  /// state of the symbol's memory information too. A range of
  /// memory-related errors can be raised.
  SPP_EXP_FUN auto ValidateSymbolMemory(
    ExpressionAst &value_ast,
    Ast const &move_ast,
    ScopeManager &sm,
    meta::CompilerMetaData *meta,
    SymbolMemoryOptions const &options = {})
    -> void;

  /// Raise if the value "sym" holds has already been moved out,
  /// reported against "use", the expression reading it.
  SPP_EXP_FUN auto RaiseIfMoved(
    VariableSymbol const &sym,
    Ast const &use,
    Scope const *scope)
    -> void;
}
