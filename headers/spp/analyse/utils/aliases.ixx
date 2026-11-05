module;
#include <spp/macros.hpp>

export module spp.analyse.utils.aliases;
import spp.analyse.scopes.type_key;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeStatementAst);

namespace spp::analyse::utils::aliases {
  /// The result of resolving a type alias, moving through multiple aliasing
  /// definitions, until a class type is reached. Include generics that need to
  /// be attached, and scope+symbol trackers.
  SPP_EXP_CLS struct AliasTarget {
    Shared<TypeAst> DirectTarget;
    Shared<GenericParameterGroupAst> AttachGenerics;
    Scope* TrackingScope;
    TypeSymbol* ResolvedSym;
  };

  /// What a type statement fully resolves to, including its target, all the
  /// arguments named, the parameters of a target alias it passes on, etc.
  /// Rejects a cyclic chain.
  SPP_EXP_FUN auto StatementTarget(
    TypeStatementAst const &alias_stmt, bool from_use_stmt,
    Scope const *tracking_scope, ScopeManager *sm, meta::CompilerMetaData *meta)
    -> AliasTarget;

  /// What an alias's instantiation stands for, by identity: the alias's target,
  /// its parameters replaced by the instantiation's arguments, as its identity
  /// ("Scope::InstanceIdOf") holds them ("Res[T=S32, E=Str]" is
  /// "Pass[S32] or Fail[Str]").
  SPP_EXP_FUN auto InstanceTargetIdOf(
    TypeSymbol const &alias, TypeId id) -> TypeId;
}
