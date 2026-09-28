module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_resolution;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeStatementAst);

namespace spp::analyse::utils::type_resolution {
  /// A type written through an alias ("Closure[(), Void]" for
  /// "type Closure[Ts, R] = FunMov[Ts, R]") as the type it stands
  /// for, arguments substituted ("FunMov[(), Void]"); any other
  /// type as it is. For readers that look at the written head,
  /// which for an alias is the alias, not its target.
  SPP_EXP_FUN auto ThroughAlias(
    TypeAst const &type,
    Scope const &scope)
    -> Shared<const TypeAst>;

  /// The core alias resolver, taking a type statement ast and
  /// determining its genuine original mapped type, untangling
  /// multi-stage aliasing, generics, etc.
  SPP_EXP_FUN auto RecursiveAliasSearch(
    TypeStatementAst const &alias_stmt,
    bool from_use_stmt,
    Scope *tracking_scope,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*>;

  /// Whether "ResolveWrittenType" replaces "Self" from the scope, or
  /// leaves it for a caller that decides per use what it stands for
  /// (function parameter and return types).
  SPP_EXP_CLS enum class SelfPolicy { kSubstitute, kKeep };

  /// Resolve a type as written in source: substitute "Self", analyse,
  /// qualify through its symbol, then restore the written convention
  /// and source span. A type that had "Self" replaced is analysed with
  /// abstract types allowed, as "Self" may name an abstract class.
  SPP_EXP_FUN auto ResolveWrittenType(
    TypeAst const &written,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    SelfPolicy self = SelfPolicy::kSubstitute)
    -> Shared<TypeAst>;

  /// Stamp every name in a type whose meaning is fixed with
  /// what it means in the scope the type is written in: a
  /// generic parameter, a closed class, and the template at
  /// the head of a name written with arguments. A copy read
  /// again from any other scope then resolves through the
  /// stamps ("Scope::Canon"), not by its spelling.
  SPP_EXP_FUN auto StampWrittenParts(
    TypeAst const &type,
    Scope const &scope)
    -> void;
}
