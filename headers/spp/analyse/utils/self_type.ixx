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
  /// "type" with "Self" read as "self": parameter 0 bound to it
  /// ("scopes::BindSelf"), both read in "scope", then analysed
  /// through "sm" when it is given. A plain, non-analysed clone
  /// where "self" is null or "type" names no "Self": only a type
  /// that had a "Self" replaced is analysed here, so a written
  /// type is not analysed at a point its owner has not chosen,
  /// and a "Self" left standing for want of an enclosing type is
  /// reported by whoever does analyse it.
  SPP_EXP_FUN auto SubstituteSelf(
    TypeAst const &type, TypeAst const *self, Scope const &scope,
    ScopeManager *sm = nullptr, meta::CompilerMetaData *meta = nullptr) -> Shared<TypeAst>;
}
