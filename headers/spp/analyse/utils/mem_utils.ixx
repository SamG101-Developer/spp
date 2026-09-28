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
  /// Gigantic analysis set on the memory status of a value,
  /// based on how we are using it (bool flags), and the current
  /// state of the symbol's memory information too. A range of
  /// memory-related errors can be raised.
  SPP_EXP_FUN auto ValidateSymbolMemory(
    ExpressionAst &value_ast,
    Ast const &move_ast,
    ScopeManager &sm,
    bool check_move,
    bool check_partial_move,
    bool check_move_from_borrowed_ctx,
    bool mark_moves,
    meta::CompilerMetaData *meta,
    bool check_escaping_borrow_move = true,
    bool place_is_written = false)
    -> void;

  /// Raise if the value "sym" holds has already been moved out,
  /// reported against "use", the expression reading it.
  SPP_EXP_FUN auto RaiseIfMoved(
    VariableSymbol const &sym,
    Ast const &use,
    Scope *scope)
    -> void;

  /// Raise if "sym" was moved, or partially moved, on only some
  /// of the paths reaching "use" (the branches of a "case", the
  /// ways out of a loop).
  SPP_EXP_FUN auto RaiseIfInconsistentlyMoved(
    VariableSymbol const &sym,
    Ast const &use,
    Scope *scope)
    -> void;

}
