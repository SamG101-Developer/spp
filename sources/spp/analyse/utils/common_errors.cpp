module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.common_errors;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts, Ast);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, TypeAst);

/// [CHECKED]
auto spp::analyse::utils::common_errors::RequirePrimary(
  ExpressionAst const &expr, ScopeManager const &sm,
  expr_utils::PrimaryExpressionOptions &&options) -> void {
  using errors::SppInvalidPrimaryExpressionError;

  // Throw an error if the result of the primary expression checker is false.
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(expr, sm, std::move(options)),
    ERR_ARGS(expr));
}

/// [CHECKED]
auto spp::analyse::utils::common_errors::RequireNotBorrowed(
  TypeAst const &type, Ast const &at, const StrView what,
  ScopeManager const &sm, const bool deep) -> void {
  // Map into the other overload, using the "type" as the "shown" type.
  RequireNotBorrowed(type, at, type, what, sm, deep);
}

/// [CHECKED]
auto spp::analyse::utils::common_errors::RequireNotBorrowed(
  TypeAst const &type, Ast const &at, Ast const &shown, const StrView what,
  ScopeManager const &sm, const bool deep) -> void {
  using errors::SppSecondClassBorrowViolationError;

  // Throw an error if the result of the type-borrowed checker is false.
  RaiseIf<SppSecondClassBorrowViolationError>(
    type_predicates::IsTypeBorrowed(type, *sm.CurrentScope, deep),
    ERR_ARGS(at, shown, what));
}

/// [CHECKED]
auto spp::analyse::utils::common_errors::RequireMutablyBorrowable(
  VariableSymbol const &sym, Ast const &mutator, ScopeManager const &sm)
  -> void {
  // Get the convention off of the symbol's type.
  using errors::SppInvalidMutationError;
  const auto c = sym.Type != nullptr ? sym.Type->GetConvention() : nullptr;
  const auto is_conv_mut = c != nullptr and *c == asts::ConventionTag::MUT;
  const auto is_conv_ref = c != nullptr and *c == asts::ConventionTag::REF;

  // For a symbol to be mutably borrowed, it must either be a mutable borrow
  // itself, or the symbol must be mutable.
  RaiseIf<SppInvalidMutationError>(
    not sym.IsMutable and not is_conv_mut,
    ERR_ARGS(
      *sym.Name, mutator,
      *memory_state::StateOf(*sm.CurrentScope, sym).AstInitialization,
      "immutable symbol"));

  // Immutable borrows, even if their symbol is mutable, cannot be borrowed
  // mutably.
  RaiseIf<SppInvalidMutationError>(
    sym.MemInfo->AstBorrowed and is_conv_ref,
    ERR_ARGS(
      *sym.Name, mutator, *sym.MemInfo->AstBorrowed, "immutable borrow"));
}
