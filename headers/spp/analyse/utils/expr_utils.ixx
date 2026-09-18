module;
#include <spp/macros.hpp>

export module spp.analyse.utils.expr_utils;
import spp.utils.types;
import sys;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct NamespaceSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct StatementAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::expr_utils {
  SPP_EXP_CLS struct PrimaryExpressionOptions {
    bool AllowTypeAst = false;
    bool AllowTokenAst = false;
  };

  /// Validate whether the use of a certain "primary expression"
  /// ast is genuinely valid in context. For example, ".." is
  /// valid in binary expressions ie ".. + tup", but not say as
  /// "f(..)".
  SPP_EXP_FUN auto IsPrimaryExprTypeValid(
    ExpressionAst const &expr,
    ScopeManager const &sm,
    PrimaryExpressionOptions &&options = {})
    -> bool;

  /// Check that there is no unreachable code by member ast
  /// checking. Ie if there are expressions after a top level
  /// terminating expression, then that is an error. Prevents
  /// any dead code at all.
  SPP_EXP_FUN auto ValidateNoUnreachableCode(
    Vec<StatementAst*> const &members,
    ScopeManager const &sm)
    -> void;

  /// Prevent statements/expressions from being used whose
  /// value is unbound and falls away. For example, just
  /// writing "1" in a function body is wrong. It is never
  /// required and often the source of bugs; prevent at
  /// compile time.
  SPP_EXP_FUN auto ValidateDiscardedValue(
    Ast &member,
    Scope *scope,
    ScopeManager const &sm,
    CompilerMetaData *meta)
    -> void;

  SPP_EXP_CLS struct DeclaringVarScope {
    sys::ssize_t Depth;
    Scope *Where;
    VariableSymbol *Symbol;
  };

  SPP_EXP_CLS struct DeclaringTypeScope {
    sys::ssize_t Depth;
    Scope *Where;
    TypeSymbol *Symbol;
  };

  SPP_EXP_CLS enum class MemberAccessForm {
    Runtime, // Accessed with "."
    Static, // Accessed with "::"
  };

  /// Whether a member can be reached using a given form. This
  /// is used to determine if the type "A" can reach "a" using
  /// "." (field) or "::" (static constant). A class can define
  /// both with the same name as they have different access.
  SPP_EXP_FUN auto MemberReachableBy(
    VariableSymbol const &sym,
    MemberAccessForm form)
    -> bool;

  /// Given a vector of scopes that are known to contains a
  /// symbol, filter them to only the ones that contain the
  /// symbol in a way that can be reached correctly.
  SPP_EXP_FUN auto MembersReachableBy(
    Vec<DeclaringVarScope> const &candidates,
    MemberAccessForm form)
    -> Vec<DeclaringVarScope>;

  /// Lookup a member in a scope, using a specific accessing
  /// method. Given there is the possibility that multiple scopes
  /// contain this (like a constant being overridden), we just
  /// use the closest scope, as it will have been guaranteed to
  /// be unique by prior analysis.
  SPP_EXP_FUN auto LookupMemberForAccess(
    Scope &type_scope,
    IdentifierAst const &name,
    MemberAccessForm form)
    -> VariableSymbol*;

  /// Starting from a given scope, search the scope, and its
  /// sup-scopes, for a given symbol, and measure how far away
  /// the containing scope is from the starting one. This
  /// allows us to check if s super-scope contains a constant
  /// that we are overriding etc.
  SPP_EXP_FUN auto ScopesDeclaringVar(
    Scope &type_scope,
    IdentifierAst const &name,
    bool sup_scope_search)
    -> Vec<DeclaringVarScope>;

  /// Starting from a given scope, search the scope, and its
  /// sup-scopes, for a given symbol, and measure how far away
  /// the containing scope is from the starting one. This
  /// allows us to check if s super-scope contains a type
  /// that we are overriding etc.
  SPP_EXP_FUN auto ScopesDeclaringType(
    Scope &type_scope,
    TypeIdentifierAst const &name,
    bool sup_scope_search)
    -> Vec<DeclaringTypeScope>;

  /// Given a search producing a vector of scope information,
  /// get the closest ones. If there is only 1, then that's
  /// fine, otherwise there is an ambiguity. For variables.
  SPP_EXP_FUN auto ClosestScopes(
    Vec<DeclaringVarScope> const &candidates)
    -> Vec<DeclaringVarScope>;

  /// Given a search producing a vector of scope information,
  /// get the closest ones. If there is only 1, then that's
  /// fine, otherwise there is an ambiguity. For types.
  SPP_EXP_FUN auto ClosestScopes(
    Vec<DeclaringTypeScope> const &candidates)
    -> Vec<DeclaringTypeScope>;

  /// Report if there is an ambiguity for variable lookup,
  /// based on the number of scopes providing a value.
  SPP_EXP_FUN auto RaiseIfAmbiguous(
    Vec<DeclaringVarScope> const &closest,
    Ast const &access,
    ScopeManager const &sm)
    -> void;

  /// Report if there is an ambiguity for type lookup, based
  /// on the number of scopes providing a value.
  SPP_EXP_FUN auto RaiseIfAmbiguous(
    Vec<DeclaringTypeScope> const &closest,
    Ast const &access,
    ScopeManager const &sm)
    -> void;

  /// Raise an error for a missing identifier, and provide the
  /// closest options; a singular place to uniformly handle
  /// these errors.
  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN
  auto RaiseMissingIdentifierAndClosestOptions(
    IdentifierAst const &identifier,
    Vec<VariableSymbol*> const &var_symbols,
    Vec<NamespaceSymbol*> const &ns_symbols,
    ScopeManager const &sm)
    -> void;

  /// Raise an error for a missing type identifier, and provide
  /// the closest options; a singular place to uniformly handle
  /// these errors.
  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN
  auto RaiseMissingTypeIdentifierAndClosestOptions(
    TypeIdentifierAst const &identifier,
    Vec<TypeSymbol*> const &symbols,
    ScopeManager const &sm)
    -> void;
}
