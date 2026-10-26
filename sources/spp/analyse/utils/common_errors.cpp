module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.common_errors;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.type_ast;
import std;

auto spp::analyse::utils::common_errors::RequirePrimary(
  ExpressionAst const &expr, ScopeManager const &sm, expr_utils::PrimaryExpressionOptions &&options)
  -> void {
  using errors::SppInvalidPrimaryExpressionError;
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(expr, sm, std::move(options)),
    ERR_ARGS(expr));
}

auto spp::analyse::utils::common_errors::RequireNotBorrowed(
  TypeAst const &type, Ast const &at, Ast const &shown, const StrView what, ScopeManager const &sm, const bool deep)
  -> void {
  using errors::SppSecondClassBorrowViolationError;
  RaiseIf<SppSecondClassBorrowViolationError>(
    type_predicates::IsTypeBorrowed(type, sm, deep),
    ERR_ARGS(at, shown, what));
}

auto spp::analyse::utils::common_errors::RequireNotBorrowed(
  TypeAst const &type, Ast const &at, const StrView what, ScopeManager const &sm, const bool deep)
  -> void {
  RequireNotBorrowed(type, at, type, what, sm, deep);
}
