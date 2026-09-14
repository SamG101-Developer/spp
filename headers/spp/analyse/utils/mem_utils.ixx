module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::utils::mem_utils, enum class MemRegionRelation);
use(spp::asts, struct Ast);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct IdentifierAst);

SPP_EXP_CLS enum class spp::analyse::utils::mem_utils::MemRegionRelation {
  Disjoint, // Non overlapping regions of memory: "a" vs "b" or "a.b" vs "a.c"
  Contains, // The first place has a region containing the second: "a" vs "a.b"
  ContainedBy, // The first place's region is contained by the second: "a.b" vs "a"
};

namespace spp::analyse::utils::mem_utils {
  /// Get the parts of a value that form the "memory region"
  /// that it represents. For example, the region "a.b.c" is
  /// represented by ["a", "b", "c"].
  SPP_EXP_FUN auto RegionPath(Ast const &ast) -> Vec<IdentifierAst*>;

  /// How 2 regions relate to one another. Compare each region
  /// for inequality implying disjointedness. If they are equal,
  /// the shorter path "contains" the longer one.
  SPP_EXP_FUN auto MemRegionRelate(Vec<IdentifierAst*> const &r1, Vec<IdentifierAst*> const &r2) -> MemRegionRelation;

  /// Two regions overlap if their relation is not "disjoint",
  /// so just wrap the memory region relation function, with
  /// "ast->region" conversion too.
  SPP_EXP_FUN auto MemRegionOverlap(Ast const &ast_1, Ast const &ast_2) -> bool;

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

  /// Gigantic analysis set on the memory status of a value,
  /// based on how we are using it (bool flags), and the current
  /// state of the symbol's memory information too. A range of
  /// memory-related errors can be raised.
  SPP_EXP_FUN auto ValidateSymbolMemory(
    ExpressionAst &value_ast,
    Ast const &move_ast,
    ScopeManager &sm,
    bool check_move,
    bool check_partial_move,
    bool check_move_from_borrowed_ctx,
    bool mark_moves,
    meta::CompilerMetaData *meta,
    bool check_escaping_borrow_move = true,
    bool place_is_written = false)
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
