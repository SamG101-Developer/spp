module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_utils;
import spp.asts.meta.compiler_meta_data;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct ExpressionAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS class VariableSymbol;
}


namespace spp::analyse::utils::mem_utils {
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
    SPP_EXP_FUN auto memory_region_overlap(
        asts::Ast const &ast_1,
        asts::Ast const &ast_2)
        -> bool;

    /**
     * This function is another, slightly more relaxed memory region overlap check. It does the same as
     * @ref memory_region_overlap, but only checks one way. This means that @c {a R_OVERLAP a.b} will result in a
     * positive match, but @c {a.b R_OVERLAP a.b} will not.
     * @param ast_1 The lhs AST to check for overlap.
     * @param ast_2 The rhs AST to check for overlap.
     * @return Whether the two memory regions overlap in the right direction.
     */
    SPP_EXP_FUN auto memory_region_right_overlap(
        asts::Ast const &ast_1,
        asts::Ast const &ast_2)
        -> bool;

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
     * @param check_pins If moving pinned objects is being checked.
     * @param check_linked_pins If moving objects linked to pins is being checked.
     * @param mark_moves Whether to mark the symbol as moved following the checks (given they pass).
     * @param meta Associated metadata.
     * @throw spp::analyse::errors::SppUninitializedMemoryUseError If the value is used before it is initialized.
     * @throw spp::analyse::errors::SppPartiallyInitializedMemoryUseError If the value is used whilst partially
     * initialized.
     * @throw spp::analyse::errors::SppMoveFromBorrowedMemoryError If the value is moved from a borrowed context.
     * @throw spp::analyse::errors::SppMoveFromPinnedMemoryError If the value is moved from a pinned context.
     * @throw spp::analyse::errors::SppMoveFromPinLinkedMemoryError If the value is moved from a context that is linked
     * to another pin.
     * @throw spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError If an inconsistently initialized symbol
     * is used.
     * @throw spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError If an inconsistently pinned symbol is used.
     */
    SPP_EXP_FUN auto validate_symbol_memory(
        asts::ExpressionAst &value_ast,
        asts::Ast const &move_ast,
        scopes::ScopeManager &sm,
        bool check_move,
        bool check_partial_move,
        bool check_move_from_borrowed_ctx,
        bool check_pins,
        bool check_linked_pins,
        bool mark_moves,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto validate_inconsistent_memory(
        asts::Ast* parent,
        std::vector<asts::CaseExpressionBranchAst*> const &branches,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
