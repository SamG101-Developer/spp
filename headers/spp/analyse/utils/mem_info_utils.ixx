module;
#include <spp/macros.hpp>

export module spp.analyse.utils.mem_info_utils;
import spp.utils.types;
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
    SPP_EXP_CLS using InconsistentCondMemState = Pair<asts::Ast*, asts::Ast*>;

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
    std::tuple<asts::Ast const*, scopes::Scope*> AstInitialization = {nullptr, nullptr};

    /**
     * The @c ast_moved AST is the AST that moved the value out of the symbol this memory information struct is attached
     * to. This attribute is only set when the symbol is in the moved state, so when a value is moved into the symbol,
     * this attribute will be set to @c nullptr. This attribute is mutually exclusive with the @c ast_initialization
     * attribute.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> AstMoved = {nullptr, nullptr};

    /**
     * The @c ast_initialization_origin AST is the same as the @c ast_initialization, but it isn't set to nullptr when
     * the symbol's value is moved out of it. This is used to track the origin of the initialization, so that when a
     * value is moved out of the symbol, the initialization origin can still be tracked and used for further analysis
     * and error formatting.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> AstInitializationOrigin = {nullptr, nullptr};

    /**
     * The @c ast_borrowed AST is the AST that is set if the value symbol is declared with a borrow type. This will be
     * set from a function parameter's convention. If this attribute is @c nullptr, then the convention is the "mov"
     * convention.
     */
    std::tuple<asts::Ast const*, scopes::Scope*> AstBorrowed = {nullptr, nullptr};

    /**
     * The @c ast_partial_moves ASTs are the ASTs that represent partial moves of the value out of the symbol. For the
     * symbol @c a, the partial moves might be @c a.b and @c a.x.y.z. Partial moves are set when attributes are moved
     * off an owned value, and are popped from this list when attributes are re-assigned to the symbol.
     */
    Vec<asts::Ast const*> AstPartialMoves;

    /**
     * Borrows that this symbol, a [coroutine/async call]-handle, contain, escaping the typical inner frame lifetime
     * constraint.
     */
    Vec<std::tuple<asts::Ast const*, bool, scopes::Scope*>> AstContainedEscapingBorrows;

    /**
     * A reverse map of the @c AstContainedEscapingBorrows, so we don't have to search each symbol for a contained
     * borrow match / overlap. The structure is "<container, where_contained>".
     */
    Vec<std::tuple<asts::Ast const*, asts::Ast const*>> AstContainersOfEscapingBorrows;

    /**
     * The @c ast_comptime AST is the AST that represents the compile-time declaration of the symbol. This might be the
     * @c cmp statement or @c cmp generic parameter, wherever the symbol was declared with @c cmp.
     * @note Must be a unique pointer due to generic arguments being dropped or converted (cheap to clone though).
     */
    Unique<asts::Ast> AstCompTime;

    /**
     * The @c initialization_counter is the number of times the symbol has been initialized. This is used for @c let
     * statements that aren't initialized on declaration, because they can still be assigned once despite not being
     * mutable.
     */
    std::size_t InitializationCounter = 0;

    /**
     * A symbol is inconsistently initialised if a symbol is <i>changed</i> into the initialised state in one branch, but
     * not in another branch. This is used to track whether a symbol has been initialised in one branch, but not in
     * another branch.
     */
    std::optional<InconsistentCondMemState> IsInconsistentlyInitialized;

    /**
     * A symbol is inconsistently moved if a symbol is <i>changed</i> into the moved state in one branch, but not in another
     * branch. This is used to track whether a symbol has been moved out of in one branch, but not in another branch.
     */
    std::optional<InconsistentCondMemState> IsInconsistentlyMoved;

    /**
     * A symbol is inconsistently partially moved if the symbol maintains different partial moves in branches. This
     * leaves the symbol in different partially moved states.
     */
    std::optional<InconsistentCondMemState> IsInconsistentlyPartiallyMoved;

    std::optional<InconsistentCondMemState> IsInconsistentlyBorrowEscaping;

    /**
     * Set the @c ast_initialization AST to the given AST, reset the @c ast_moved AST to @c nullptr, and remove all
     * partial moves. Increment the @c initialization_counter by 1. This marks the symbol as being in the fully
     * initialized state.
     * @param ast The AST that initialized the symbol.
     * @param scope The scope in which the initialization occurred.
     */
    auto InitializedBy(asts::Ast const &ast, scopes::Scope *scope) -> void;

    /**
     * Set the @c ast_moved AST to the given AST, reset the @c ast_initialization AST to @c nullptr. This marks the
     * symbol as being in the moved state.
     * @param ast The AST that moved the value out of the symbol.
     * @param scope The scope in which the move occurred.
     */
    auto MovedBy(asts::Ast const &ast, scopes::Scope *scope) -> void;

    /**
     * Remove a partial move from the symbol's memory information. This is used when a partial move is re-assigned to
     * the symbol, so that the symbol is no longer in a partially moved state.
     * @param ast The AST that represents the partial move to remove.
     * @param scope The scope in which the partial move removal occurred.
     */
    auto RemovePartialMoves(asts::Ast const &ast, scopes::Scope *scope) -> void;

    /**
     * Create a snapshot of the current memory information. This is used to capture the state of the memory at a given
     * time.
     * @return The snapshot of the memory information.
     */
    SPP_ATTR_NODISCARD auto Snapshot() const -> MemoryInfoSnapshot;

    /**
     * Provide a clone method for the @c MemoryInfo struct. This is used to create a clone of the memory information for
     * when a symbol is cloned.
     * @return A unique pointer to the cloned @c MemoryInfo.
     * @note The clone is not a deep copy, so pointers will be shared between the original and the clone.
     */
    SPP_ATTR_NODISCARD auto Clone() const -> Unique<MemoryInfo>;

    /**
     * Fill the memory information from a given snapshot. This is used to restore the memory information to a previous
     * state.
     * @param snapshot The snapshot to restore the memory information from.
     */
    auto FillFromSnapshot(MemoryInfoSnapshot const &snapshot) -> void;
};

SPP_EXP_CLS struct spp::analyse::utils::mem_info_utils::MemoryInfoSnapshot {
    /**
     * View of the initializing ast for the owning @c MemoryInfo
     */
    asts::Ast const *AstInitialization;

    /**
     * The scope in which the initializing ast for the owning @c MemoryInfo was present at the time of the snapshot.
     */
    scopes::Scope *ScopeInitialization = nullptr;

    /**
     * View of the moving ast for the owning @c MemoryInfo at the time of the snapshot.
     */
    asts::Ast const *AstMoved;

    /**
     * The scope for the moving ast for the owning @c MemoryInfo at the time of the snapshot.
     */
    scopes::Scope *ScopeMoved;

    /**
     * List of partial moves that were present in the owning @c MemoryInfo at the time of the snapshot.
     */
    Vec<asts::Ast const*> AstPartialMoves;

    /**
     * List of escaping borrows that were present in the owning @c MemoryInfo at the time of the snapshot.
     */
    Vec<std::tuple<asts::Ast const*, bool, scopes::Scope*>> AstContainedEscapingBorrows;

    /**
     * The @c initialization_counter that was present in the owning @c MemoryInfo at the time of the snapshot.
     */
    std::size_t InitializationCounter;
};
