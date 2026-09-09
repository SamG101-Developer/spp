module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct CaseExpressionBranchAst;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct FunctionCallArgumentAst;
  SPP_EXP_CLS struct IdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS class VariableSymbol;
}

namespace spp::analyse::utils::mem_utils {
  SPP_EXP_CLS enum class MemRegionRelation {
    Disjoint, // Non overlapping regions of memory: "a" vs "b" or "a.b" vs "a.c"
    Contains, // The first place has a region containing the second: "a" vs "a.b"
    ContainedBy, // The first place's region is contained by the second: "a.b" vs "a"
  };

  SPP_EXP_FUN auto RegionPath(
    asts::Ast const &ast)
    -> Vec<asts::IdentifierAst*>;

  SPP_EXP_FUN auto MemRegionRelate(
    asts::Ast const &region,
    Vec<Str> const &steps)
    -> MemRegionRelation;

  /**
   * Two memory regions overlap, if one of the symbols is a strict subset of the other. Sharing a common owner does
   * not guarantee an overlap. For example, @c a overlaps with @c a. This is the most basic overlap example.
   * The symbol @c a.b also overlaps with @c a, as @c a.b is a subset of @c a. However, @c a.b and @c a.c do not
   * overlap, as they are different parts of a common owning symbol, without any shared regions.
   *
   * This function checks that both ASTs don't overlap each other, so both @c {a.b OVERLAP? a} and @c {a OVERLAP? a.b}
   * will result in a positive match.
   * @param ast_1 The lhs AST to check for overlap.
   * @param ast_2 The rhs AST to check for overlap.
   * @return Whether the two memory regions overlap.
   */
  SPP_EXP_FUN auto MemRegionOverlap(
    asts::Ast const &ast_1,
    asts::Ast const &ast_2)
    -> bool;

  /**
   * Account for the borrow @p arg takes when the borrow has no name, and raise if it meets one already held.
   * @param arg The argument to account for.
   * @param sym The argument's outermost symbol, or null when it has none. A non-null one returns immediately: that
   * borrow is named, and so is either taken by the caller's own branches or is a borrow being passed along rather
   * than a second one taken here - the @c self of a @c {&mut self} method is the latter.
   * @param[in,out] borrows_ref The immutable borrows the argument list holds so far.
   * @param[in,out] borrows_mut The mutable borrows the argument list holds so far.
   * @param sm The scope manager, for the argument's type and for the scope an error is reported against.
   * @param meta Associated metadata, for the argument's type.
   */
  SPP_EXP_FUN auto ValidateUnnamedArgumentBorrow(
    asts::FunctionCallArgumentAst const &arg,
    scopes::VariableSymbol const *sym,
    Vec<asts::Ast const*> &borrows_ref,
    Vec<asts::Ast const*> &borrows_mut,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  /**
   * Many memory checks are performed here by analysing the ASTs present in the value's symbol, to ensure that memory
   * errors can be detected and prevented at compile time. There are flags for almost all the checks, as there are
   * scenarios where some of these checks need to be ignored.
   * @param value_ast The AST whose memory is being checked.
   * @param move_ast The AST performing the move operation.
   * @param sm The scope manager to get symbol's memory information from.
   * @param check_move If a full move is being checked for validity.
   * @param check_partial_move If a partial move is being checked for validity.
   * @param check_move_from_borrowed_ctx If moving from a borrowed context is being checked.
   * @param mark_moves Whether to mark the symbol as moved following the checks (given they pass).
   * @param meta Associated metadata.
   * @throw spp::analyse::errors::SppUninitializedMemoryUseError If the value is used before it is initialized.
   * @throw spp::analyse::errors::SppPartiallyInitializedMemoryUseError If the value is used whilst partially
   * initialized.
   * @throw spp::analyse::errors::SppMoveFromBorrowedMemoryError If the value is moved from a borrowed context.
   * @throw spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError If an inconsistently initialized symbol
   * is used.
   * @param check_escaping_borrow_move Whether moving a value that carries escaping borrows is refused outright. Left
   * on everywhere the destination goes unweighed; turned off by a caller that follows this with
   * @c PreventBorrowLifetimeExtension , which compares the destination's lifetime against the borrows' own and is the
   * more precise answer.
   */
  SPP_EXP_FUN auto ValidateSymbolMemory(
    asts::ExpressionAst &value_ast,
    asts::Ast const &move_ast,
    scopes::ScopeManager &sm,
    bool check_move,
    bool check_partial_move,
    bool check_move_from_borrowed_ctx,
    bool mark_moves,
    asts::meta::CompilerMetaData *meta,
    bool check_escaping_borrow_move = true)
    -> void;


  SPP_EXP_FUN auto PreventBorrowLifetimeExtension(
    asts::Ast const &rhs_expr,
    scopes::VariableSymbol const *lhs_outermost,
    scopes::VariableSymbol const *rhs_outermost,
    asts::Ast *owner,
    scopes::ScopeManager const &sm,
    bool override_borrow = false)
    -> void;
}
