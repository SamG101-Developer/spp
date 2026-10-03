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

  /// The literal a comp identity's value spells ("2_uz", "-3_s32", "true"), as an ast; nothing for anything else.
  SPP_EXP_FUN auto CompValueAst(StrView literal) -> Unique<ExpressionAst>;


  /// A constant named through a closed type ("Buf::n", "Box[S32]::n"), read as its identity: the value it folds to,
  /// else the member itself ("scopes::MemberCompKey") when its value names a parameter or something opaque.
  /// Nothing when the constant cannot be reached yet (its "sup" block is not attached, and the template's blocks do
  /// not declare it): zero. Installed as "scopes::CompMembers::Find" at compiler boot, and read only through
  /// "scopes::CompMemberIdOf".
  SPP_EXP_FUN auto FindCompMemberId(scopes::TypeId owner, StrView name) -> scopes::CompId;

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

  /// The identity of a comp-time value, read from "scope", parsed
  /// ("scopes::CompNode"; "Scope::CompIdOf" interns it):
  /// a closed value is the literal it folds to, a comp generic is
  /// its parameter, parentheses are looked through, and an
  /// operation over them is written fully bracketed - so
  /// "(n + 1)" and "n + 1" are one value, "(a + b) * c" and
  /// "a + (b * c)" are not, and two scopes binding "n" apart are
  /// two values. Anything else is its spelling. The type twin is
  /// "Scope::TypeKey".
  SPP_EXP_FUN auto CompKey(ExpressionAst const &expr, Scope const &scope) -> scopes::CompNode;

  /// The value an opaque part of a comp identity stands for ("O<len>:<spelling>"), recorded as it was keyed
  /// ("CompKey"); null for an identity never keyed. No identity names such a value, so this is how
  /// "Scope::CompAstOf" names it again.
  SPP_EXP_FUN auto OpaqueCompValue(scopes::CompId id) -> ExpressionAst const*;

  /// Forget every recorded opaque value: each is stamped with one compilation's parameters, which are freed after it
  /// ("ScopeManager::Cleanup").
  SPP_EXP_FUN auto ClearOpaqueCompValues() -> void;

}
