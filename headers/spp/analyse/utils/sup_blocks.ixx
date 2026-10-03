module;
#include <spp/macros.hpp>

export module spp.analyse.utils.sup_blocks;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TypeAst);

/// What the two "sup" blocks ("sup X { .. }", "sup X ext Y { .. }") do alike at each stage, each called with the scope
/// manager inside the block's own scope.
namespace spp::analyse::utils::sup_blocks {
  /// Stage 2: a block's generic parameters may not be optional, and each must be named, by identity, in one of
  /// "naming_types" (the block's type, and an extension's supertype), so it can be inferred from them. With no naming
  /// types (a "$" mock block, whose parameters are the function's own), only the first is checked.
  SPP_EXP_FUN auto CheckGnParams(
    GenericParameterGroupAst const &params,
    Vec<TypeAst const*> const &naming_types,
    ScopeManager const &sm)
    -> void;

  /// Register "Self" as what "name" names: a class prototype's own name, or a block's type once qualified (through an
  /// alias, its target). A "$" mock registers none.
  SPP_EXP_FUN auto RegisterSelf(
    TypeAst const &name,
    ScopeManager &sm)
    -> void;

  /// Stage 3: register a provisional "Self" before any alias in the block is resolved, so that one naming it has
  /// something to resolve to. The name is not qualified yet, so its head symbol answers; a name that does not
  /// resolve registers none (the stage qualifying it reports it). "LoadTarget" replaces it.
  SPP_EXP_FUN auto RegisterProvisionalSelf(
    TypeAst const &name,
    ScopeManager &sm)
    -> void;

  /// Stage 5: analyse the block's type ("name", abstract allowed: a block is where abstract methods are declared and
  /// implemented), reject a borrow, qualify it in place, file the block against its base symbol (a module-level block,
  /// or any "$" mock block when "files_nested_mock"), and re-register "Self" against the qualified name. "block" is
  /// where an error points. Answers the base symbol.
  SPP_EXP_FUN auto LoadTarget(
    Ast const &block,
    Shared<TypeAst> &name,
    bool files_nested_mock,
    ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> TypeSymbol*;
}
