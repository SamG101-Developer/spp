module;
#include <spp/macros.hpp>

export module spp.analyse.utils.sup_attachment;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts::meta, struct CompilerMetaData);

/// Attaching each type's super scopes - the "sup" blocks over it,
/// and the generic ones that apply to it - checking each generic
/// constraint as it goes. Over a compile's "ScopeTree", through any
/// manager walking it.
namespace spp::analyse::utils::sup_attachment {
  /// For every type discovered up to this point, attach the
  /// designated supertypes to it, checking each generic
  /// constraint as it is attached. A constraint is checked
  /// against the constrained type's own supertypes, which may
  /// not be attached yet, so reading a scope's supertypes during
  /// the sweep attaches them first ("Scope::OnSupScopesRead").
  SPP_EXP_FUN auto AttachAllSuperScopes(
    ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// Attach the super scopes to one type, once ("Scope::SupsAttached"),
  /// returning whether it attached them now. Called by the sweep,
  /// and when a generic class is instantiated.
  SPP_EXP_FUN auto AttachSpecificSuperScopes(
    Scope &scope, ScopeManager const &sm, CompilerMetaData *meta) -> bool;

  /// Forget the sup statements gathered per scope, for the end of
  /// a compile ("ScopeTree").
  SPP_EXP_FUN auto ClearSupStatementsCache() -> void;
}
