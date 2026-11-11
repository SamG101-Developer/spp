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
    type_predicates::IsTypeBorrowed(type, *sm.CurrentScope, deep),
    ERR_ARGS(at, shown, what));
}

auto spp::analyse::utils::common_errors::RequireNotBorrowed(
  TypeAst const &type, Ast const &at, const StrView what, ScopeManager const &sm, const bool deep)
  -> void {
  RequireNotBorrowed(type, at, type, what, sm, deep);
}

auto spp::analyse::utils::common_errors::RequireMutablyBorrowable(
  VariableSymbol const &sym, Ast const &mutator, ScopeManager const &sm)
  -> void {
  using errors::SppInvalidMutationError;
  auto const *const conv = sym.Type != nullptr ? sym.Type->GetConvention() : nullptr;

  // An immutable symbol cannot be lent on mutably, unless what it
  // holds is itself a mutable borrow.
  RaiseIf<SppInvalidMutationError>(
    not sym.IsMutable and not (conv != nullptr and *conv == asts::ConventionTag::MUT),
    ERR_ARGS(
      *sym.Name, mutator, *memory_state::StateOf(*sm.CurrentScope, sym).AstInitialization, "immutable symbol"));

  // Immutable borrows, even if their symbol is mutable, cannot be
  // lent on mutably.
  RaiseIf<SppInvalidMutationError>(
    sym.MemInfo->AstBorrowed and conv != nullptr and *conv == asts::ConventionTag::REF,
    ERR_ARGS(*sym.Name, mutator, *sym.MemInfo->AstBorrowed, "immutable borrow"));
}
