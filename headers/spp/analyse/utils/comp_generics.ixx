module;
#include <spp/macros.hpp>

export module spp.analyse.utils.comp_generics;
import spp.analyse.scopes.type_key;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::analyse::utils::comp_generics, struct CompTimeResolutionGuard);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterGroupAst);
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

  /// Give each comp argument its parameter's type to be analysed against ("GenericArgumentAst::ExpectedCompType"),
  /// where the parameter it binds ("type_resolution::ParamsBoundByArgs") has one of its own ("cmp n: [Opt[U64]; 2_uz]",
  /// not "cmp n: T", nor a variadic one), read where "params" is declared.
  SPP_EXP_FUN auto ExpectCompParamTypes(
    GenericArgumentGroupAst const &args, GenericParameterGroupAst const &params, Scope const &params_scope) -> void;

  /// A comp value with a function call in it ("ct()", "n + ct()"),
  /// evaluated as a "cmp" statement's value is: the literal it
  /// folds to, running any "cmp fun" it calls, or an E3 for one
  /// that is no comp-time value (a runtime function's call). A
  /// call keys only as its spelling, so is folded where it is
  /// written. So is an operation over values its key leaves
  /// unfolded ("1_uz / 0_uz"), as the call it desugars to, for
  /// the error that call raises. Nullptr for anything else, or
  /// in a template, which each instantiation folds for itself.
  /// "value" is analysed already.
  SPP_EXP_FUN auto FoldCompCall(
    ExpressionAst &value, ScopeManager *sm, asts::meta::CompilerMetaData *meta) -> Unique<ExpressionAst>;

  /// Whether a comp value (or an element of it) is an operation,
  /// which is analysed on a copy, as its operator call.
  SPP_EXP_FUN auto IsCompOperator(
    ExpressionAst const &value) -> bool;
}

/// A "cmp" constant being resolved at compile time, for as long
/// as the guard lives, including when it is left by a thrown
/// error. A constant whose value reaches itself again - through a
/// name ("a") or through a type ("Self::a"), on one stack - is a
/// cycle with no value: "IsResolving" tells the reference that
/// would enter it again, which raises rather than recursing.
SPP_EXP_CLS struct spp::analyse::utils::comp_generics::CompTimeResolutionGuard {
  explicit CompTimeResolutionGuard(VariableSymbol const *sym);
  CompTimeResolutionGuard(CompTimeResolutionGuard const &) = delete;
  CompTimeResolutionGuard(CompTimeResolutionGuard &&) = delete;
  auto operator=(CompTimeResolutionGuard const &) -> CompTimeResolutionGuard& = delete;
  auto operator=(CompTimeResolutionGuard &&) -> CompTimeResolutionGuard& = delete;
  ~CompTimeResolutionGuard();

  /// Whether "sym" is being resolved, further up the stack.
  SPP_ATTR_NODISCARD static auto IsResolving(VariableSymbol const *sym) -> bool;
};
