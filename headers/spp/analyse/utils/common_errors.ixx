module;
#include <spp/macros.hpp>

export module spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TypeAst);

/// The checks every kind of expression or type position makes, that belong to
/// no one area: each raises its error where the check fails.
namespace spp::analyse::utils::common_errors {
  /// Ensure a primary expression is a valid type of primary expression; the
  /// token ".." can only be used in binary expressions, and types can only be
  /// used if they are zero-types.
  SPP_EXP_FUN auto RequirePrimary(
    ExpressionAst const &expr, ScopeManager const &sm,
    expr_utils::PrimaryExpressionOptions &&options = {}) -> void;

  /// Check that a type is not a borrow type, ie a type with a "&" or "&mut"
  /// convention applied. This is used to enforce second-class borrow rules in
  /// the memory and type systems.
  SPP_EXP_FUN auto RequireNotBorrowed(
    TypeAst const &type, Ast const &at, StrView what, ScopeManager const &sm,
    bool deep = true) -> void;

  /// Another second-class borrow checker for types, but allowing for a separate
  /// "written" type to be passed in, for error reporting. Called by the above
  /// function with "shown" set to "type".
  SPP_EXP_FUN auto RequireNotBorrowed(
    TypeAst const &type, Ast const &at, Ast const &shown, StrView what,
    ScopeManager const &sm, bool deep = true) -> void;

  /// Check that a symbol can be mutably borrowed, without as mutation failure
  /// based on the definitions mutability, and without a conflict of borrows,
  /// enforcing the law of exclusivity.
  SPP_EXP_FUN auto RequireMutablyBorrowable(
    VariableSymbol const &sym, Ast const &mutator, ScopeManager const &sm)
    -> void;
}
