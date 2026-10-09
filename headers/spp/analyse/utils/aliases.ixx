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
  /// What a type statement names: its immediate target as written,
  /// arguments named (an alias of an alias names that alias, read
  /// through it by identity), the parameters of a target alias it
  /// passes on, the scope the class at the end of the chain was
  /// found in (where an instantiation is attached), and that class.
  /// Rejects a cyclic chain.
  SPP_EXP_FUN auto StatementTarget(
    TypeStatementAst const &alias_stmt, bool from_use_stmt, Scope *tracking_scope,
    ScopeManager *sm, meta::CompilerMetaData *meta)
    -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*, TypeSymbol*>;

  /// What an alias's instantiation stands for: the alias's target,
  /// its parameters replaced by the instantiation's arguments, as
  /// its identity ("Scope::InstanceIdOf") holds them ("Res[T=S32,
  /// E=Str]" is "Pass[S32] or Fail[Str]"), built in "scope". Null
  /// when the identity or the target has no such reading.
  SPP_EXP_FUN auto InstanceTargetOf(
    TypeSymbol const &alias, TypeId id, Scope const &scope) -> Shared<TypeAst>;
}
