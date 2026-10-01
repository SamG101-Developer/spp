module;
#include <spp/macros.hpp>

export module spp.analyse.utils.comp_generics;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);

namespace spp::analyse::utils::comp_generics {
  /// Fold a comp-time value to a literal where that needs no
  /// analysis: a literal (spelled canonically), parentheses, a
  /// comp generic bound to such a value, and integer arithmetic,
  /// bit operations and comparisons over them, through the same
  /// comp-time intrinsics a "cmp" function runs. Null when the
  /// value is not closed (it names an unbound generic) or is not
  /// one of these shapes.
  SPP_EXP_FUN auto FoldCompExpr(ExpressionAst const &expr, Scope const &scope) -> Unique<ExpressionAst>;

  /// What a comp argument written as "expr" stands for where
  /// "scope" reads it: the value it folds to, else the value the
  /// comp generic it names is bound to ("SizedInteger[w]" in a
  /// "[cmp w: U32]" instance stands for 32). Null when neither,
  /// and it stays as written.
  SPP_EXP_FUN auto ResolveCompArg(ExpressionAst const &expr, Scope const &scope) -> Unique<ExpressionAst>;

  /// Whether any name in a comp expression satisfies "pred": the
  /// names inside an operation, a parenthesis or a pack too. The comp
  /// analog of "TypeAst::AnyPart", which walks a type's parts.
  SPP_EXP_FUN auto AnyCompName(
    ExpressionAst const &expr, std::function<bool(IdentifierAst const &)> const &pred) -> bool;

  /// Record on every comp generic named in a comp-time value -
  /// through parentheses and the operands of an operation, not
  /// only a bare name - the parameter it names where the
  /// value is written, so a copy carried into another scope
  /// keeps naming it ("Scope::CanonVar").
  SPP_EXP_FUN auto RecordCompGenerics(ExpressionAst const &expr, Scope const &scope) -> void;

  /// Whether a comp-time value names a comp parameter that
  /// "scope" does not bind - through parentheses and the operands
  /// of an operation - so it is no value until instantiated.
  SPP_EXP_FUN auto NamesCompParam(ExpressionAst const &expr, Scope const &scope) -> bool;

  /// Append the identity of a comp-time value, read from "scope",
  /// to "out": a closed value is the literal it folds to, a comp
  /// generic is its parameter, parentheses are looked through,
  /// and an operation over them is written fully bracketed - so
  /// "(n + 1)" and "n + 1" are one value, "(a + b) * c" and
  /// "a + (b * c)" are not, and two scopes binding "n" apart
  /// are two values. Anything else is its spelling. Appends, so
  /// an operation's operands write into the one buffer and a
  /// caller keying many arguments reuses it.
  /// Todo: this will change from string to identity key soon.
  SPP_EXP_FUN auto CompExprIdentity(ExpressionAst const &expr, Scope const &scope, Str &out) -> void;

  /// A comp generic's identity from its symbol, as "Scope::TypeIdOfSym"
  /// keys a type generic's: a binding is the value it is bound to, a
  /// parameter (or a binding to nothing) is itself ("C<ParamId>").
  SPP_EXP_FUN auto CompIdentityOfSym(
    VariableSymbol const &sym, Scope const &scope, Str &out) -> void;

}
