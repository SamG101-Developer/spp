module;
#include <spp/macros.hpp>

export module spp.analyse.utils.expr_utils;
import spp.utils.types;
import std;
import sys;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct StatementAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::expr_utils {
  /// Options for fine-tuning how the primary expression checker works. Extend
  /// with more fields if required (to block other asts).
  SPP_EXP_CLS struct PrimaryExpressionOptions {
    bool AllowTypeAst = false;
  };

  /// A value read more than once, and also evaluated only once via "HoistOnce".
  /// The temporary it was bound to, and that temporary's name, which is read in
  /// its "place". Both null for a "place".
  SPP_EXP_CLS struct Hoisted {
    Unique<LetStatementInitializedAst> Let;
    Shared<IdentifierAst> Name;
  };

  /// Validate whether the use of a "primary expression" ast is genuinely valid
  /// in context. For example, ".." works in binary expressions ie ".. + pack",
  /// but not say as "f(..)".
  SPP_EXP_FUN auto IsPrimaryExprTypeValid(
    ExpressionAst const &expr, ScopeManager const &sm,
    PrimaryExpressionOptions &&options = {}) -> bool;

  /// Whether an expression names existing storage: a variable, or a field path
  /// off one ("t", "t.items"). Anything else ("g()", "x?") makes a new value.
  SPP_EXP_FUN auto IsPlace(
    ExpressionAst const &value, Scope const &scope) -> bool;

  /// Hoisting takes what would be a temporary, and materializes it, allowing
  /// multiple analysis passes to use the same named storage (a "place"). Needed
  /// for the middle of "a < b < c" for example, and several other places.
  SPP_EXP_FUN auto HoistOnce(
    ExpressionAst const &value, StrView uid_prefix, ScopeManager *sm,
    CompilerMetaData *meta, bool analyse = true) -> Hoisted;


  /// Get "tuple.i", un-analysed. This an element read off a tuple (or a pack,
  /// in a template) by position.
  SPP_EXP_FUN auto ElementAccess(
    ExpressionAst const &tuple,
    std::size_t index) -> Unique<PostfixExpressionAst>;

  /// A spread of a tuple expanded ("ExpandTupleSpread"): its elements, and the
  /// temporary they are read off when the tuple is no place ("HoistOnce").
  SPP_EXP_CLS struct TupleSpread {
    /// "let $spread.<uid> = <tuple>", analysed: a tuple that is no place
    /// ("..g()") is evaluated once, into it, by whoever holds the elements,
    /// before them. None for a place ("t", "self.items"), read where it is.
    Unique<LetStatementInitializedAst> Temp;

    /// "t.0", "t.1", ...: each element, read off the place or the temporary.
    Vec<Unique<ExpressionAst>> Elems;
  };

  /// What a spread of a tuple ("..t") stands for: its elements. A borrowed
  /// tuple ("t" a borrow) is expanded as the tuple it borrows. None for an open
  /// tuple (a template's pack, "Tup[Ts]"), which has no length to expand to, so
  /// stays a spread. E19 at "tok" for a value that is no tuple.
  SPP_EXP_FUN auto ExpandTupleSpread(
    ExpressionAst &tuple, Ast const &tok, ScopeManager *sm,
    CompilerMetaData *meta) -> Opt<TupleSpread>;

  /// Tell "member", a statement whose value nothing takes, before
  /// it is analysed: a "case" (through a block's final member)
  /// then lets its branches disagree on type, so a value in one is
  /// reported as discarded ("DiscardValue"), not as a
  /// type mismatch with the others.
  SPP_EXP_FUN auto MarkValueDiscarded(
    Ast &member) -> void;

  /// The value of "owner"'s member "i" goes nowhere: a temporary is
  /// dropped once it is made ("SetDropsDiscarded"), unless its type
  /// is "!must_use" (E95). A "case" or a block written as the member
  /// discards each value it ends on, in the block that ends on it.
  SPP_EXP_FUN auto DiscardValue(
    InnerScopeExpressionAst &owner, std::size_t i,
    ScopeManager const &sm, CompilerMetaData *meta) -> void;

}
