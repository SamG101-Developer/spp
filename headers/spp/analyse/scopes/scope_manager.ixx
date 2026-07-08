module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope_range;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import ankerl.unordered_dense;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS struct ModulePrototypeAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::utils::errors {
    SPP_EXP_CLS class ErrorFormatter;
}

/**
 * The @c ScopeManager type is critical for the semantic analysis of the entire program being compiled. It contains the
 * scopes of the code, providing the structure of the code, and the symbols within those scopes. It also provides an
 * iterator to traverse the scopes in a depth-first manner, which is useful for various analysis tasks.
 */
SPP_EXP_CLS class spp::analyse::scopes::ScopeManager {
    /**
     * The current iterator state. This is a coroutine-based iterator that allows for depth-first traversal of the
     * scopes. It is reset with the @c reset method, and advanced with the @c move_to_next_scope and
     * @c create_and_move_into_new_scope methods.
     */
    ScopeIterator _It;

    Unique<asts::ClassPrototypeAst> _SelfProto;

public:
    /**
     * The static list of sup blocks that have been processed. This attribute is defined as static so because all
     * created @c ScopeManager instances will share it. This allows any @c ScopeManager to analyse types and inject the
     * sup block logic into the manager.
     */
    inline static ankerl::unordered_dense::map<TypeSymbol*, Vec<Scope*>> normal_sup_blocks = {};

    /**
     * This list contains the pure generic sup blocks, such as
     * @code
     * sup [T] T { ... }
     * @endcode
     * The list is separate as there is some special handling required.
     */
    inline static Vec<Scope*> generic_sup_blocks = {};

    inline static Vec<Unique<Scope>> temp_scopes = {};

    /**
     * The global scope is the root scope fo the entire program. It is a @c Shared as temp scope manager's need
     * to be created sometimes, where the global scope will be shared. Not a raw pointer as the scope managers do own
     * the global scope.
     */
    Shared<Scope> GlobalScope;

    /**
     * A @c Scope type owns its children with @c Unique types, so the @c Scope stored by the manager as the
     * "current_scope" will be a raw pointer to the scope.
     */
    Scope *CurrentScope = nullptr;

    /**
     * Create a new @c ScopeManager object. The current scope will default to the global scope if not provided. The
     * global scope is co-owned by this @c ScopeManager, and any other active managers. The current scope will be owned
     * by its parent scope, so is a raw pointer.
     * @param global_scope The global scope for the program.
     * @param current_scope The current scope that this manager will "start" in.
     */
    explicit ScopeManager(
        Shared<Scope> const &global_scope,
        Scope *current_scope = nullptr);

    ~ScopeManager();

    /**
     * Iterate the @c ScopeManager from the current scope downwards in a depth-first manner. Each child scope will be
     * visited before sibling scopes. A new iterator will be created each time this function is called. The
     * @c ScopeRange type is returned, which in turn has @c begin and @c end functions to provide the actual
     * iterators. This separates the iterator logic from the manager logic.
     * @return The @c ScopeRange type, which can be used to get the @c begin and @c end iterators.
     */
    SPP_ATTR_NODISCARD auto Iter() const -> ScopeRange;

    /**
     * Reset this @c ScopeManager to the provided scope. If no scope is provided, it will reset to the global scope.
     * This moves the current scope to this scope, and resets the iterator to the same point. An iterator can be
     * provided which might already be part-iterated, so an exact state can be provided.
     * @param scope The scope to reset to. If @c nullptr, the global scope will be used.
     * @param iterator The iterator state to reset to. If @c std::nullopt, the iterator will be reset to the start of
     * the provided scope.
     */
    auto Reset(
        Scope *scope = nullptr,
        std::optional<ScopeIterator> iterator = std::nullopt)
        -> void;

    /**
     * Create a new @c Scope as the child of the current scope, and move into it. The iterator is updated to point to
     * the new scope. Because the iterator is coroutine-based, the new scope will be the next scope visited. The name of
     * the scope must be provided, and can be a n @c Str, @c asts::IdentifierAst or @c asts::TypeIdentifierAst.
     * @param name The name of the new scope.
     * @param ast The optional AST node that this scope represents. This will typically be a top-level AST node, such as
     * a function or class prototype.
     * @param error_formatter The optional error formatter to use for this scope. If @c nullptr, the parent's error
     * formatter will be used. This provides the correct token set to the error builder if this scope's parent module
     * contains any errors in.
     * @return The new scope that has been created and moved into. A raw pointer is returned as the scope is owned by
     * its parent scope.
     */
    auto CreateAndMoveIntoNewScope(
        ScopeName const &name,
        asts::Ast *ast = nullptr,
        utils::errors::ErrorFormatter *error_formatter = nullptr)
        -> Scope*;

    /**
     * Move out of the current scope into the parent scope. This is used after a scope has been moved into for a
     * function or class, so that following code can be applied to the outer scope.
     * @return The parent scope that has been moved into. A raw pointer is returned as the scope is owned by its parent
     * scope. This is never called when the global scope is the current scope, so the return value is never @c nullptr.
     */
    auto MoveOutOfCurrentScope()
        -> Scope*;

    /**
     * Move into the next scope in the iteration. This will move into child scopes before sibling scopes, in a
     * depth-first manner. If the end of the iteration has been reached, an error will be thrown because the coroutine
     * will be exhausted. This doesn't ever create a new scope, it just moves into the next one in the iteration.
     * @return The next scope in the iteration. A raw pointer is returned as the scope is owned by its parent scope. If
     * the end of the iteration has been reached, an error will be thrown.
     */
    auto MoveToNextScope(bool ignore_alias_class_scopes = true)
        -> Scope*;

    /**
     * Skip every scope belonging to the current scope. This moves the iterator such that iterating once more will move
     * to the next sibling of this scope.
     */
    auto ExhaustScope() -> void;

    auto AttachLlvmTypeInfo(
        asts::ModulePrototypeAst const &mod,
        codegen::LLvmCtx *ctx) const
        -> void;

    /**
     * A super scope attachment whose generic constraint has not yet been verified. During the bulk
     * @c AttachAllSuperScopes pass, constrained super scopes are attached structurally (without checking their
     * constraints) so that attachment is order-independent; the constraints are then validated afterwards, once
     * every type has its super scopes, and any attachment whose constraint is unsatisfied is pruned.
     */
    struct DeferredSupConstraint {
        Scope *owner_scope;    ///< The scope the super scope was attached to (pruned if the constraint fails).
        Scope *sup_scope;      ///< The (specialized) super scope that was attached.
        Scope *sup_cls_scope;  ///< The paired super class scope also attached, or nullptr.
        Scope *base_sup_scope; ///< The original (constrained) generic sup block.
    };

    /**
     * For every type discovered up until this point, attach the defined supertypes to them. At this point, all base
     * classes, and @c sup blocks, will have been injected into the symbol table, but possibly not all generic
     * substitutions of some of these type. This is fine because the @c TypeAst semantic analysis will call individual
     * sup scope attachment functions if needed.
     *
     * Attachment happens in two phases so that it is independent of the order types are visited in. Phase 1 attaches
     * every super scope structurally, deferring the check of any generic constraint (eg @c {I: Zero} on
     * @c {sup [V, I: Zero] SliceMut[V, I]}). Constraint checking walks the constrained type's own super scopes, which
     * may not have been attached yet if that type is visited later, so checking inline would be order-dependent.
     * Phase 2 validates the deferred constraints once every type has its super scopes, and prunes any attachment whose
     * constraint is unsatisfied (repeated to a fixpoint so transitive constraint chains resolve).
     * @param meta The compiler metadata.
     */
    auto AttachAllSuperScopes(
        asts::meta::CompilerMetaData *meta)
        -> void;

    /**
     * The public method to attach all the superscopes to an individual type, represented by its corresponding scope.
     * This is the function called by the type analysis code if the generic substitution is new. For example, of
     * @c Vec[Str] has been discovered after the "attach all" was performed, then this function will be called to attach
     * the superscopes of @c Vec[Str].
     * @param scope The scope representing the type to attach superscopes to.
     * @param meta The compiler metadata.
     * @param deferred When non-null (the bulk @c AttachAllSuperScopes pass), generic constraints are not checked
     * inline; instead each constrained attachment is recorded here for later validation. When null (on-demand
     * attachment of a single newly-discovered type), constraints are checked inline as before.
     */
    auto AttachSpecificSuperScopes(
        Scope &scope,
        asts::meta::CompilerMetaData *meta,
        Vec<DeferredSupConstraint> *deferred = nullptr) const
        -> void;

private:
    /**
     * The implementation logic for attaching specific super scopes to a type scope. This is separated out so that the
     * public method can alter scope lists based on @c TypeSymbol vs @c AliasSymbol, where-as this function strictly
     * applies the scope changes.
     * @param scope The scope representing the type to attach superscopes to.
     * @param sup_scopes The list of super scopes to attach to the type scope. This is passed in as an rvalue reference
     * because it will typically be created on-the-fly by the public method.
     * @param meta The compiler metadata.
     */
    auto AttachSpecificSuperScopesImpl(
        Scope &scope,
        Vec<Scope*> &&sup_scopes,
        asts::meta::CompilerMetaData *meta,
        Vec<DeferredSupConstraint> *deferred) const
        -> void;

    /**
     * Validate the generic constraints of the super scope attachments deferred during phase 1 of
     * @c AttachAllSuperScopes, and prune any attachment whose constraint is not satisfied. This runs after every type
     * has had its super scopes attached, so the constraint checks (which walk the constrained type's super scopes) are
     * order-independent. It is repeated to a fixpoint, because pruning one attachment can invalidate the constraint of
     * another (transitive constraint chains).
     * @param deferred The deferred constrained attachments recorded during phase 1.
     * @param meta The compiler metadata.
     */
    auto PruneUnsatisfiedSupConstraints(
        Vec<DeferredSupConstraint> &deferred,
        asts::meta::CompilerMetaData *meta) const
        -> void;

    /**
     * Once a super scope has been registered against a scope, there are some checks to ensure that this new superscope
     * is semantically compatible with the type. There can be no conflicting @c use or @c cmp statements with existing
     * definitions on existing supertypes of the type.
     * @param cls_sym The type symbol representing the class to check.
     * @param sup_scope The new super scope that has been added to the class.
     */
    static auto CheckConflictingTypeOrCmpStatements(
        TypeSymbol const &cls_sym,
        Scope const &sup_scope)
        -> void;

public:
    auto CurrentIterator() -> ScopeIterator&;

    auto SelfProto() const -> asts::ClassPrototypeAst*;

    /**
     * Reset the static state of the @c ScopeManager. This clears the static lists of sup blocks, so that a new
     * compilation can be started from a clean state. This should be called at the end of a full compilation. This is
     * required so that the unit tests can run different code as "main" in the same process.
     */
    static auto Cleanup() -> void;
};
