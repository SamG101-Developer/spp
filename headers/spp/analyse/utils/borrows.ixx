module;
#include <spp/macros.hpp>

export module spp.analyse.utils.borrows;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct FunctionCallArgumentAst);

namespace spp::analyse::utils::borrows {
  /// "conflict" with "loan", met now: raised at once, unless the
  /// loan's holder acts only when it is used (a borrow binding, a
  /// generator, a closure), when it is recorded on the loan and
  /// raised only if the holder is used again ("RaiseIfConflictedUse"):
  /// a loan lasts to its holder's last use (M9.2). A holder that can
  /// act on its own (a future or a thread running in the background)
  /// keeps its loans to the end of its scope.
  SPP_EXP_FUN auto ConflictWith(
    memory_state::Loan &loan, memory_state::LoanConflict const &conflict,
    ScopeManager const &sm) -> void;

  /// Raise the conflict recorded on a loan "holder" holds, if one
  /// is: the holder is being used again, so the loan was live.
  SPP_EXP_FUN auto RaiseIfConflictedUse(
    VariableSymbol const &holder, Scope const &scope) -> void;

  /// Raise if "place", borrowed by "use" ("exclusive" for a "&mut"
  /// borrow or a move, otherwise a "&" borrow), overlaps a loan the
  /// call being checked holds ("call_loans"), and record the
  /// conflict on any loan of the body ("loans") it overlaps
  /// ("ConflictWith"): an exclusive use overlaps any loan, a "&"
  /// one only a "&mut" loan.
  SPP_EXP_FUN auto CheckAgainstLoans(
    regions::PlaceAt const &place, bool exclusive,
    memory_state::LoanSet &loans, memory_state::LoanSet const &call_loans,
    Ast const &use, ScopeManager const &sm) -> void;

  /// Detect if an unnamed argument borrow conflicts with the
  /// normal borrows from a function call. This basically means
  /// a non-symbolic borrow being validated. This is seen where
  /// we might have something like "f(&a, a[mut 5])", so a[mut 5]
  /// does mutably borrow into "a" ie "&mut a". The symbol is
  /// the outermost for the arg, so "a".
  SPP_EXP_FUN auto ValidateUnnamedArgumentBorrow(
    FunctionCallArgumentAst const &arg, VariableSymbol const *sym,
    memory_state::LoanSet &call_loans, ScopeManager &sm,
    meta::CompilerMetaData *meta) -> void;

  /// Raise if "holder", just given a value ("owner": a "let" or
  /// an assignment), would outlive what it now borrows: the root
  /// of any loan the value carries (held by the holder, or still
  /// by "from" where they were not handed over) may not be
  /// declared in a scope nested inside the holder's. "from" is
  /// the binding the value came from, if any: a borrow binding
  /// holding no loan (a pattern's "&val") may not be either.
  SPP_EXP_FUN auto EnforceBorrowsOutliveHolder(
    VariableSymbol const &holder, VariableSymbol const *from,
    Ast const &owner, ScopeManager const &sm) -> void;
}
