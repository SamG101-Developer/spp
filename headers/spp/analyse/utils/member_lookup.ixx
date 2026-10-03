module;
#include <spp/macros.hpp>

export module spp.analyse.utils.member_lookup;
import spp.utils.ptr;
import spp.utils.types;
import std;
import sys;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct NamespaceSymbol);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

namespace spp::analyse::utils::member_lookup {
  SPP_EXP_CLS enum class MemberAccessForm {
    Runtime, // Accessed with "."
    Static, // Accessed with "::"
  };

  SPP_EXP_CLS struct DeclaringTypeScope {
    sys::ssize_t Depth;
    Scope *Where;
    TypeSymbol *Symbol;
  };

  SPP_EXP_CLS struct DeclaringVarScope {
    sys::ssize_t Depth;
    Scope *Where;
    VariableSymbol *Symbol;
  };

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

  /// "Scope::FindTypeSymbol" for "type" in "scope", raising an unknown-identifier error (with the closest names) for
  /// its last part when there is none.
  SPP_EXP_FUN auto FindTypeSymbolOrError(
    Scope const &scope,
    TypeAst const &type,
    ScopeManager const &sm)
    -> TypeSymbol*;

  /// "FindTypeSymbolOrError" for a namespace: the symbol "ns" names in "scope".
  SPP_EXP_FUN auto FindNsSymbolOrError(
    Scope const &scope,
    IdentifierAst const &ns,
    ScopeManager const &sm)
    -> NamespaceSymbol*;

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

  /// The member "name" of the type whose scope is "type_scope", as "form" reaches it: the type's own or its super
  /// scopes', never a lexical parent's (the lookup is exclusive). The nearest declaration "form" can reach; else, where
  /// only one it cannot reach exists, that one (for the caller to report as the wrong form). Null for no such member.
  /// Prior analysis has made the nearest one unique ("RaiseIfAmbiguous").
  SPP_EXP_FUN auto MemberOf(
    Scope &type_scope,
    IdentifierAst const &name,
    MemberAccessForm form)
    -> VariableSymbol*;

  /// The nearest declarations of the member "name" that "form" can reach ("ScopesDeclaringVar", "MembersReachableBy",
  /// "ClosestScopes"): one, or several at the same depth, which is an ambiguity.
  SPP_EXP_FUN auto ClosestMembers(
    Scope &type_scope,
    IdentifierAst const &name,
    MemberAccessForm form)
    -> Vec<DeclaringVarScope>;

  /// Every scope that declares the variable "name" itself: the type's scope and each of its super scopes (of any
  /// level), each asked on its own, with how far it is from the type's scope. Each entry's "Where" is the scope that
  /// declares the symbol, so a super scope's member is listed once, against that super scope.
  SPP_EXP_FUN auto ScopesDeclaringVar(
    Scope &type_scope,
    IdentifierAst const &name)
    -> Vec<DeclaringVarScope>;

  /// "ScopesDeclaringVar" for a type "name".
  SPP_EXP_FUN auto ScopesDeclaringType(
    Scope &type_scope,
    TypeIdentifierAst const &name)
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

}
