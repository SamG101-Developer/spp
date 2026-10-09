module;
#include <spp/macros.hpp>

export module spp.analyse.utils.comp_generics;
import spp.analyse.scopes.type_key;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::comp_generics {
  /// Read a constant, such as "Buf::n" or "Box[S32]::n", and
  /// hand out the comp id. Either the value's identity (if
  /// foldable), so to something like "4_uz"; the member's own
  /// identity, where the constant exists but the value isn't
  /// plain (uses another parameter/holds something opaque/not
  /// foldable); or null. "declared_in" is the block a "Self::n"
  /// is written in, read where the owner's blocks do not declare
  /// the constant yet (not attached to it until stage 5 ends).
  SPP_EXP_FUN auto FindCompMemberId(
    TypeId owner, StrView name, Scope const *declared_in = nullptr) -> CompId;

  /// Whether a comp value needs the "sup" scopes attached before
  /// it can be typed: it (or, for a pack's tuple, any element)
  /// is more than a bare literal or name. A literal is typed by
  /// its suffix and a name by its declaration, from stage 4; an
  /// operation ("n + 1_uz", the call "n.add(1_uz)") or a
  /// constant named through a type ("Foo::N") is found through
  /// the "sup" scopes, attached once stage 5 ends, so it is
  /// analysed and type-checked only after they are.
  SPP_EXP_FUN auto NeedsSupScopesToType(
    ExpressionAst const &value) -> bool;

  /// Whether a comp value (or an element of it) is an operation,
  /// which is analysed on a copy, as its operator call.
  SPP_EXP_FUN auto IsCompOperator(
    ExpressionAst const &value) -> bool;
}
