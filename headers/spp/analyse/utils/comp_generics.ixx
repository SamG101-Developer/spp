module;
#include <spp/macros.hpp>

export module spp.analyse.utils.comp_generics;
import spp.analyse.scopes.instance_key;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct CompNode);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::comp_generics {
  /// Fold a comp-time value to a literal where that needs no
  /// analysis: a literal (spelled canonically), parentheses, a
  /// comp generic bound to such a value, and integer arithmetic,
  /// bit operations and comparisons over them, through the same
  /// comp-time intrinsics a "cmp" function runs. Null when the
  /// value is not closed (it names an unbound generic) or is not
  /// one of these shapes.
  SPP_EXP_FUN auto FoldCompExpr(ExpressionAst const &expr, Scope const &scope) -> Unique<ExpressionAst>;

  /// A comp identity's value as its literal ("2_uz", "-3_s32", "true"); nothing for anything else.
  SPP_EXP_FUN auto CompValueAst(CompNode const &value) -> Unique<ExpressionAst>;

  /// A constant named through a closed type ("Buf::n", "Box[S32]::n"), read as its identity: the value it folds to,
  /// else the member itself ("scopes::CompNode::OfMember") when its value names a parameter or something opaque.
  /// Nothing when the constant cannot be reached yet (its "sup" block is not attached, and the template's blocks do
  /// not declare it): null. Installed as "scopes::CompMembers::Find" at compiler boot, and read only through
  /// "scopes::CompMemberIdOf".
  SPP_EXP_FUN auto FindCompMemberId(TypeId owner, StrView name) -> CompId;

  /// Whether a comp value (or an element of it, a pack's tuple) is more than a literal or a name: an operation, or a
  /// constant named through a type. Its type, and so its value, needs the "sup" scopes attached, so it is analysed and
  /// type-checked only after they are.
  SPP_EXP_FUN auto IsCompExpression(ExpressionAst const &value) -> bool;

  /// Whether a comp value (or an element of it) is an operation, which is analysed on a copy, as its operator call.
  SPP_EXP_FUN auto IsCompOperator(ExpressionAst const &value) -> bool;

  /// "TypeAst::SubstituteSelf" for a comp value: every type it names a constant through ("Self::N"), or holds as a
  /// value, has its "Self" replaced by "with", through the parts a comp value is made of; anything else is kept as
  /// written.
  SPP_EXP_FUN auto SubstituteCompSelf(ExpressionAst const &value, TypeAst const &with) -> Shared<ExpressionAst>;
}
