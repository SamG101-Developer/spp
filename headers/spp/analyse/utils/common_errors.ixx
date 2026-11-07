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

/// The checks every kind of expression or type position makes, that belong to no one area: each raises its error
/// where the check fails.
namespace spp::analyse::utils::common_errors {
  /// "expr" can stand where a value is read ("expr_utils::IsPrimaryExprTypeValid"), or an
  /// "SppInvalidPrimaryExpressionError" at it.
  SPP_EXP_FUN auto RequirePrimary(
    ExpressionAst const &expr, ScopeManager const &sm, expr_utils::PrimaryExpressionOptions &&options = {})
    -> void;

  /// "type" is not a borrow ("type_predicates::IsTypeBorrowed", "deep" as there), or an
  /// "SppSecondClassBorrowViolationError" at "at", showing "shown" as the type and "what" as the position: borrows
  /// are second class, so a field, element or return type cannot hold one.
  SPP_EXP_FUN auto RequireNotBorrowed(
    TypeAst const &type, Ast const &at, Ast const &shown, StrView what, ScopeManager const &sm, bool deep = true)
    -> void;

  /// "RequireNotBorrowed" showing the checked type itself.
  SPP_EXP_FUN auto RequireNotBorrowed(
    TypeAst const &type, Ast const &at, StrView what, ScopeManager const &sm, bool deep = true)
    -> void;

  /// "sym" can be lent on mutably by "mutator" (the "&mut" of a call's argument or a "gen"): it is "mut", or holds a
  /// mutable borrow ("x: &mut S32"), and holds no immutable one; or an "SppInvalidMutationError".
  SPP_EXP_FUN auto RequireMutablyBorrowable(
    VariableSymbol const &sym, Ast const &mutator, ScopeManager const &sm)
    -> void;
}
