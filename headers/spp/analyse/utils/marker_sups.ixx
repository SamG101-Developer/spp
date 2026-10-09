module;
#include <spp/macros.hpp>

export module spp.analyse.utils.marker_sups;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::marker_sups {
  /// The function type ("FunRef" / "FunMut" / "FunMov") the type is or superimposes, looked at through an alias;
  /// none for a type that is not callable. A generic's constraint is considered first: a "FunRef" constrained to be a
  /// "FunMov" is treated as the "FunMov" would be, for consistent behaviour.
  SPP_EXP_FUN auto FindFnSup(
    TypeAst const &type,
    Scope const &scope)
    -> TypeRef;

  /// The generator ("Gen" / "GenOnce") the type is or superimposes: its "Yield" argument is what a resumption gives
  /// ("GenYieldOf"). None for no generator, or more than one, raised on "expr" (printing the type "spell" gives, only
  /// asked for then) unless "raise" is false.
  SPP_EXP_FUN auto FindGenSup(
    TypeRef const &ref,
    Scope const &scope,
    ExpressionAst const &expr,
    std::function<Shared<TypeAst>()> const &spell,
    StrView what,
    bool raise = true)
    -> TypeRef;

  /// What a generator found by "FindGenSup" yields: its "Yield"
  /// argument, off its identity ("TypeArgRef"). No type for no
  /// generator; name it with "AstIn" where syntax or a message
  /// needs it.
  SPP_EXP_FUN auto GenYieldOf(TypeRef const &gen) -> TypeRef;

  /// Whether a generator found by "FindGenSup" is a "GenOnce".
  SPP_EXP_FUN auto IsGenOnce(TypeRef const &gen, Scope const &scope) -> bool;

  /// Resuming a finished "Gen" answers "GenDone", so a yield type
  /// holding it would be indistinguishable from that. Raised on
  /// "expr" when it does. A "GenOnce" has no sentinel, so is
  /// never checked. Basically prevent "GenDone" from manually
  /// appearing - always injected.
  SPP_EXP_FUN auto EnforceYieldTypeWithoutGenDone(
    TypeRef const &gen,
    Scope const &scope,
    Ast const &expr,
    StrView what)
    -> void;

  /// The "Try" the type is or superimposes, whose "Value" and "Residual" arguments are what an early return reads.
  /// None for no "Try", or more than one, raised as "FindGenSup" does.
  SPP_EXP_FUN auto FindTrySup(
    TypeRef const &ref,
    ExpressionAst const &expr,
    std::function<Shared<TypeAst>()> const &spell,
    ScopeManager const &sm,
    StrView what,
    bool raise = true)
    -> TypeRef;

  /// The forwarding superimpositions the type is or superimposes: the first "FwdRef" and the first "FwdMut", none for
  /// a kind it does not have. Each forwards to its "T" ("FwdTargetOf": "StrView" for "Str", which superimposes
  /// "FwdRef[T=StrView]").
  SPP_EXP_FUN auto FindFwdSups(
    TypeRef const &ref,
    Scope const &scope)
    -> Pair<TypeRef, TypeRef>;

  /// What a forwarding superimposition found by "FindFwdSups" forwards to: its "T". None for no superimposition.
  SPP_EXP_FUN auto FwdTargetOf(TypeRef const &fwd) -> TypeRef;

  /// Manually build the hidden forwarding call that is
  /// abstracted over for things like member access, assignment,
  /// returning etc. This is needed to the llvm codegen can
  /// access the forwarded object.
  SPP_EXP_FUN auto BuildFwdCall(
    ExpressionAst const &receiver,
    TypeRef const &receiver_ref,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Unique<PostfixExpressionAst>;

  /// As above, taking the receiver itself rather than a copy, for
  /// an argument whose analysed value becomes the forwarding call's.
  SPP_EXP_FUN auto BuildFwdCall(
    Unique<ExpressionAst> &&receiver,
    TypeRef const &receiver_ref,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Unique<PostfixExpressionAst>;

  /// Whether a value of this type forwards at all ("FwdRef" or
  /// "FwdMut"), which is when "BuildFwdCall" builds a call.
  SPP_EXP_FUN auto CanForward(
    TypeRef const &receiver_ref,
    Scope const &scope)
    -> bool;

}
