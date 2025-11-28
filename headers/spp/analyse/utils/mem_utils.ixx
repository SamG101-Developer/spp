module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_utils;
import spp.asts.meta.compiler_meta_data;
import std;


namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ExpressionAst;
}


namespace spp::analyse::utils::mem_utils {
    /**
     * The @c InconsistentCondMemState holds two @c CondMemState pairs, which have conflicting information about the
     * symbols from different branches. Branches must equally set memory information for symbols if the branch is being
     * used for assignment.
     */
    SPP_EXP_CLS using InconsistentCondMemState = std::pair<asts::Ast*, asts::Ast*>;

    SPP_EXP_CLS struct MemoryInfo;
    SPP_EXP_CLS struct MemoryInfoSnapshot;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS class VariableSymbol;
}


/**
 * The MemoryInfo struct is used to track the memory state of a symbol in the scope. It contains information about the
 * initialization, movement, and borrowing of the symbol's value, as well as any partial moves and pins that may be
 * associated with it.
 */
SPP_EXP_CLS struct spp::analyse::utils::mem_utils::MemoryInfo {
    /**
     * The @c ast_initialization AST is the AST that initialized the symbol this memory information struct is attached
     * to. This attribute is only set when the symbol is in the initialization state, so when a value is moved out of
     * the symbol, this attribute will be set to @c nullptr. This attribute is mutually exclusive with the @c ast_moved
     * attribute.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> ast_initialization = {nullptr, nullptr};

    /**
     * The @c ast_moved AST is the AST that moved the value out of the symbol this memory information struct is attached
     * to. This attribute is only set when the symbol is in the moved state, so when a value is moved into the symbol,
     * this attribute will be set to @c nullptr. This attribute is mutually exclusive with the @c ast_initialization
     * attribute.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> ast_moved = {nullptr, nullptr};

    /**
     * The @c ast_initialization_origin AST is the same as the @c ast_initialization, but it isn't set to nullptr when
     * the symbol's value is moved out of it. This is used to track the origin of the initialization, so that when a
     * value is moved out of the symbol, the initialization origin can still be tracked and used for further analysis
     * and error formatting.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> ast_initialization_origin = {nullptr, nullptr};

    /**
     * The @c ast_borrowed AST is the AST that is set if the value symbol is declared with a borrow type. This will be
     * set from a function parameter's convention. If this attribute is @c nullptr, then the convention is the "mov"
     * convention.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> ast_borrowed = {nullptr, nullptr};

    /**
     * The @c ast_partial_moves ASTs are the ASTs that represent partial moves of the value out of the symbol. For the
     * symbol @c a, the partial moves might be @c a.b and @c a.x.y.z. Partial moves are set when attributes are moved
     * off an owned value, and are popped from this list when attributes are re-assigned to the symbol.
     */
    std::vector<asts::Ast const*> ast_partial_moves;

    /**
     * The @c ast_pins are the ASTs that are either this symbol or partials of this symbol that are pinned from borrows
     * into coroutines or async function calls. Certain operations cannot be performed on pinned symbols, such as moving
     * them, or re-initializing them.
     */
    std::vector<asts::Ast const*> ast_pins;

    /**
     * A linked pin in a pin that is caused via assignment. For example, @code let x = coro(&y)@endcode will cause @c x
     * to be a linked pin for @y, because @c y is pinned due to being borrowed into the coroutine that is assigned to
     * @c x. The boolean determines if the pin from a mutable borrow.
     */
    std::vector<std::tuple<scopes::VariableSymbol*, bool, scopes::Scope*>> ast_linked_pins;

    /**
     * The @c ast_comptime AST is the AST that represents the compile-time declaration of the symbol. This might be the
     * @c cmp statement or @c cmp generic parameter, wherever the symbol was declared with @c cmp.
     * @note Must be a unique pointer due to generic arguments being dropped or converted (cheap to clone though).
     */
    std::unique_ptr<asts::Ast> ast_comptime;

    /**
     * The @c initialization_counter is the number of times the symbol has been initialized. This is used for @c let
     * statements that aren't initialized on declaration, because they can still be assigned once despite not being
     * mutable.
     */
    std::size_t initialization_counter = 0;

    /**
     * A symbol is inconsistently initialised if a symbol is <i>changed</i> into the initialised state in one branch, but
     * not in another branch. This is used to track whether a symbol has been initialised in one branch, but not in
     * another branch.
     */
    std::optional<InconsistentCondMemState> is_inconsistently_initialized;

    /**
     * A symbol is inconsistently moved if a symbol is <i>changed</i> into the moved state in one branch, but not in another
     * branch. This is used to track whether a symbol has been moved out of in one branch, but not in another branch.
     */
    std::optional<InconsistentCondMemState> is_inconsistently_moved;

    /**
     * A symbol is inconsistently partially moved if the symbol maintains different partial moves in branches. This
     * leaves the symbol in different partially moved states.
     */
    std::optional<InconsistentCondMemState> is_inconsistently_partially_moved;

    /**
     * A symbol is inconsistently pinned if the symbol maintains different pin states in branches. This leaves the
     * symbol in different pinned states.
     */
    std::optional<InconsistentCondMemState> is_inconsistently_pinned;

    /**
     * Set the @c ast_initialization AST to the given AST, reset the @c ast_moved AST to @c nullptr, and remove all
     * partial moves. Increment the @c initialization_counter by 1. This marks the symbol as being in the fully
     * initialized state.
     * @param ast The AST that initialized the symbol.
     * @param scope The scope in which the initialization occurred.
     */
    auto initialized_by(asts::Ast const &ast, scopes::Scope *scope) -> void;

    /**
     * Set the @c ast_moved AST to the given AST, reset the @c ast_initialization AST to @c nullptr. This marks the
     * symbol as being in the moved state.
     * @param ast The AST that moved the value out of the symbol.
     * @param scope The scope in which the move occurred.
     */
    auto moved_by(asts::Ast const &ast, scopes::Scope *scope) -> void;

    /**
     * Remove a partial move from the symbol's memory information. This is used when a partial move is re-assigned to
     * the symbol, so that the symbol is no longer in a partially moved state.
     * @param ast The AST that represents the partial move to remove.
     * @param scope The scope in which the partial move removal occurred.
     */
    auto remove_partial_move(asts::Ast const &ast, scopes::Scope *scope) -> void;

    /**
     * Create a snapshot of the current memory information. This is used to capture the state of the memory at a given
     * time.
     * @return The snapshot of the memory information.
     */
    SPP_ATTR_NODISCARD auto snapshot() const -> MemoryInfoSnapshot;

    /**
     * Provide a clone method for the @c MemoryInfo struct. This is used to create a clone of the memory information for
     * when a symbol is cloned.
     * @return A unique pointer to the cloned @c MemoryInfo.
     * @note The clone is not a deep copy, so pointers will be shared between the original and the clone.
     */
    SPP_ATTR_NODISCARD auto clone() const -> std::unique_ptr<MemoryInfo>;
};


SPP_EXP_CLS struct spp::analyse::utils::mem_utils::MemoryInfoSnapshot {
    /**
     * View of the initializing ast for the owning @c MemoryInfo
     */
    asts::Ast const *ast_initialization;

    /**
     * View of the moving ast for the owning @c MemoryInfo
     */
    asts::Ast const *ast_moved;

    /**
     * List of partial moves that were present in the owning @c MemoryInfo at the time of the snapshot.
     */
    std::vector<asts::Ast const*> ast_partial_moves;

    /**
     * List of pins that were present in the owning @c MemoryInfo at the time of the snapshot.
     */
    std::vector<asts::Ast const*> ast_pins;

    /**
     * List of linked pins that were present in the owning @c MemoryInfo at the time of the snapshot.
     */
    std::vector<std::tuple<scopes::VariableSymbol*, bool, scopes::Scope*>> ast_linked_pins;

    /**
     * The @c initialization_counter that was present in the owning @c MemoryInfo at the time of the snapshot.
     */
    std::size_t initialization_counter;
};


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

    SPP_EXP_CLS template <typename T>
    auto validate_inconsistent_memory(
        std::vector<T> const &branches,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
