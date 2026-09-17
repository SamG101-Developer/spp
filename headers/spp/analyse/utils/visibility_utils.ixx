module;
#include <spp/macros.hpp>

export module spp.analyse.utils.visibility_utils;
import spp.asts.utils.visibility;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct IdentifierAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::visibility_utils {
  /// Check if a type member is accessible from where it is
  /// being named. The non-throwing core of normal type member
  /// visibility.
  SPP_EXP_FUN auto IsTypeMemberVisible(
    VariableSymbol const &sym,
    Scope const &type_scope,
    ScopeManager const &sm,
    CompilerMetaData const &meta)
    -> bool;

  /// Check if a type member is accessible from where it is
  /// being named. Throws an error on bad visibility, uses the
  /// type member core.
  SPP_EXP_FUN auto CheckTypeMemberVisibility(
    VariableSymbol const &sym,
    Ast const &access_ast,
    Scope const &type_scope,
    ScopeManager const &sm,
    CompilerMetaData const &meta)
    -> void;

  /// Check if a type's nested type is accessible from where it
  /// is being named. Throws an error on bad visibility, uses
  /// the (internal) type member core.
  SPP_EXP_FUN auto CheckTypeTypeVisibility(
    TypeSymbol const &sym,
    Ast const &access_ast,
    Scope const &type_scope,
    ScopeManager const &sm,
    CompilerMetaData const &meta)
    -> void;


  /// Check if a module member is accessible from where it is
  /// being named. Throws an error on bad visibility, uses the
  /// (internal) module member core.
  SPP_EXP_FUN auto CheckModuleMemberVisibility(
    VariableSymbol const &sym,
    Ast const &access_ast,
    Scope const &definition_scope,
    ScopeManager const &sm,
    CompilerMetaData const &meta)
    -> void;

  /// Check if a module type member is accessible from where it
  /// is being named. Throws an error on bad visibility, uses
  /// the (internal) module member core.
  SPP_EXP_FUN auto CheckModuleTypeVisibility(
    TypeSymbol const &sym,
    Ast const &access_ast,
    Scope const &definition_scope,
    ScopeManager const &sm,
    CompilerMetaData const &meta)
    -> void;
}
