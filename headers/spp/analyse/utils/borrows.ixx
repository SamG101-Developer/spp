module;
#include <spp/macros.hpp>

export module spp.analyse.utils.borrows;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct FunctionCallArgumentAst);

namespace spp::analyse::utils::borrows {
  /// Detect if an unnamed argument borrow conflicts with the
  /// normal borrows from a function call. This basically means
  /// a non-symbolic borrow being validated. This is seen where
  /// we might have something like "f(&a, a[mut 5])", so a[mut 5]
  /// does mutably borrow into "a" ie "&mut a". The symbol is
  /// the outermost for the arg, so "a".
  SPP_EXP_FUN auto ValidateUnnamedArgumentBorrow(
    FunctionCallArgumentAst const &arg,
    VariableSymbol const *sym,
    Vec<Ast const*> &borrows_ref,
    Vec<Ast const*> &borrows_mut,
    ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> void;

  /// In an assignment operation, if we have "a" and "b" being
  /// borrows, we can only assign "b" into "a", if the lifetime
  /// of "b" is >= that of "a" (determined by the scope where
  /// the borrow was created). This prevents a borrow from a
  /// destructure escaping its scope when being assigned into a
  /// borrow from the function param for example.
  SPP_EXP_FUN auto PreventBorrowLifetimeExtension(
    Ast const &rhs_expr,
    VariableSymbol const *lhs_outermost,
    VariableSymbol const *rhs_outermost,
    Ast *owner,
    ScopeManager const &sm,
    bool override_borrow = false)
    -> void;

}
