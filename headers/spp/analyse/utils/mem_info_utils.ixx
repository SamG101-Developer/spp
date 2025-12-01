module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_info_utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ExpressionAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct VariableSymbol;
}


namespace spp::analyse::utils::mem_info_utils {
    /**
     * The @c InconsistentCondMemState holds two @c CondMemState pairs, which have conflicting information about the
     * symbols from different branches. Branches must equally set memory information for symbols if the branch is being
     * used for assignment.
     */
    SPP_EXP_CLS using InconsistentCondMemState = std::pair<asts::Ast*, asts::Ast*>;

    SPP_EXP_CLS struct MemoryInfo;
    SPP_EXP_CLS struct MemoryInfoSnapshot;
}


/**
 * The MemoryInfo struct is used to track the memory state of a symbol in the scope. It contains information about the
 * initialization, movement, and borrowing of the symbol's value, as well as any partial moves and pins that may be
 * associated with it.
 */
SPP_EXP_CLS struct spp::analyse::utils::mem_info_utils::MemoryInfo {
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


SPP_EXP_CLS struct spp::analyse::utils::mem_info_utils::MemoryInfoSnapshot {
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
