module;
#include <spp/macros.hpp>

export module spp.analyse.utils.self_type;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::self_type {
  /// Resolve the "Self" type for the scope, and do a substitution
  /// on the type to translate all the generics into the true type.
  SPP_EXP_FUN auto SubstituteSelfType(
    TypeAst const &type,
    Scope const &scope,
    meta::CompilerMetaData const &meta,
    bool *substituted = nullptr)
    -> Shared<TypeAst>;

  /// Resolve the "Self" type for the scope, and do a substitution
  /// on the type to translate all the generics into the true type.
  /// Do an analysis afterwards if a substitution actually happened,
  /// which "substituted" reports.
  SPP_EXP_FUN auto SubstituteSelfTypeAndAnalyse(
    TypeAst const &type,
    Scope const &scope,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    bool *substituted = nullptr)
    -> Shared<TypeAst>;

  /// Replace every "Self" part of a written type with a type given
  /// outright, for the callers that decide what "Self" stands for
  /// themselves rather than reading it off the scope - overload
  /// resolution picks between the type owning the function and the
  /// type at the call site's receiver.
  SPP_EXP_FUN auto SubstituteSelfTypeWith(
    TypeAst const &type,
    TypeAst const &replacement)
    -> Shared<TypeAst>;

}
