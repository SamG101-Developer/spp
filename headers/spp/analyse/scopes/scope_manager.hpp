#pragma once
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_iterator.hpp>
#include <spp/analyse/scopes/scope_range.hpp>
#include <spp/utils/error_formatter.hpp>

/// @cond
namespace spp::analyse::scopes {
    class ScopeManager;
    struct TypeSymbol;
}

namespace spp::asts::mixins {
    struct CompilerMetaData;
}

/// @endcond


/**
 * The @c ScopeManager type is critical for the semantic analysis of the entire program being compiled. It contains the
 * scopes of the code, providing the structure of the code, and the symbols within those scopes. It also provides an
 * iterator to traverse the scopes in a depth-first manner, which is useful for various analysis tasks.
 */
class spp::analyse::scopes::ScopeManager {
    friend struct asts::TypeStatementAst;

private:
    /**
     * The current iterator state. This is a coroutine-based iterator that allows for depth-first traversal of the
     * scopes. It is reset with the @c reset method, and advanced with the @c move_to_next_scope and
     * @c create_and_move_into_new_scope methods.
     */
    ScopeIterator m_it;

public:
    /**
     * The static list of sup blocks that have been processed. This attribute is defined as static so because all
     * created @c ScopeManager instances will share it. This allows any @c ScopeManager to analyse types and inject the
     * sup block logic into the manager.
     */
    inline static std::map<TypeSymbol*, std::vector<Scope*>> normal_sup_blocks = {};

    /**
     * This list contains the pure generic sup blocks, such as
     * @code
     * sup [T] T { ... }
     * @endcode
     * The list is separate as there is some special handling required.
     */
    inline static std::vector<Scope*> generic_sup_blocks = {};

    /**
     * The global scope is the root scope fo the entire program. It is a @c std::shared_ptr as temp scope manager's need
     * to be created sometimes, where the global scope will be shared. Not a raw pointer as the scope managers do own
     * the global scope.
     */
    std::shared_ptr<Scope> global_scope;

    /**
     * A @c Scope type owns its children with @c std::unique_ptr types, so the @c Scope stored by the manager as the
     * "current_scope" will be a raw pointer to the scope.
     */
    Scope *current_scope = nullptr;

public:
    /**
     * Create a new @c ScopeManager object. The current scope will default to the global scope if not provided. The
     * global scope is co-owned by this @c ScopeManager, and any other active managers. The current scope will be owned
     * by its parent scope, so is a raw pointer.
     * @param global_scope The global scope for the program.
     * @param current_scope The current scope that this manager will "start" in.
     */
    explicit ScopeManager(
        std::shared_ptr<Scope> const &global_scope,
        Scope *current_scope = nullptr);

    /**
     * Iterate the @c ScopeManager from the current scope downwards in a depth-first manner. Each child scope will be
     * visited before sibling scopes. A new iterator will be created each time this function is called. The
     * @c ScopeRange type is returned, which in turn has @c begin and @c end functions to provide the actual
     * iterators. This separates the iterator logic from the manager logic.
     * @return The @c ScopeRange type, which can be used to get the @c begin and @c end iterators.
     */
    SPP_ATTR_NODISCARD auto iter() const -> ScopeRange;

    /**
     * Reset this @c ScopeManager to the provided scope. If no scope is provided, it will reset to the global scope.
     * This moves the current scope to this scope, and resets the iterator to the same point. An iterator can be
     * provided which might already be part-iterated, so an exact state can be provided.
     * @param scope The scope to reset to. If @c nullptr, the global scope will be used.
     * @param iterator The iterator state to reset to. If @c std::nullopt, the iterator will be reset to the start of
     * the provided scope.
     */
    auto reset(
        Scope *scope = nullptr,
        std::optional<ScopeIterator> iterator = std::nullopt)
        -> void;

    /**
     * Create a new @c Scope as the child of the current scope, and move into it. The iterator is updated to point to
     * the new scope. Because the iterator is coroutine-based, the new scope will be the next scope visited. The name of
     * the scope must be provided, and can be a n @c std::string, @c asts::IdentifierAst or @c asts::TypeIdentifierAst.
     * @param name The name of the new scope.
     * @param ast The optional AST node that this scope represents. This will typically be a top-level AST node, such as
     * a function or class prototype.
     * @param error_formatter The optional error formatter to use for this scope. If @c nullptr, the parent's error
     * formatter will be used. This provides the correct token set to the error builder if this scope's parent module
     * contains any errors in.
     * @return The new scope that has been created and moved into. A raw pointer is returned as the scope is owned by
     * its parent scope.
     */
    auto create_and_move_into_new_scope(
        ScopeName const &name,
        asts::Ast *ast = nullptr,
        spp::utils::errors::ErrorFormatter *error_formatter = nullptr)
        -> Scope*;

    /**
     * Move out of the current scope into the parent scope. This is used after a scope has been moved into for a
     * function or class, so that following code can be applied to the outer scope.
     * @return The parent scope that has been moved into. A raw pointer is returned as the scope is owned by its parent
     * scope. This is never called when the global scope is the current scope, so the return value is never @c nullptr.
     */
    auto move_out_of_current_scope()
        -> Scope*;

    /**
     * Move into the next scope in the iteration. This will move into child scopes before sibling scopes, in a
     * depth-first manner. If the end of the iteration has been reached, an error will be thrown because the coroutine
     * will be exhausted. This doesn't ever create a new scope, it just moves into the next one in the iteration.
     * @return The next scope in the iteration. A raw pointer is returned as the scope is owned by its parent scope. If
     * the end of the iteration has been reached, an error will be thrown.
     * @throw
     */
    SPP_NO_ASAN auto move_to_next_scope()
        -> Scope*;

    /**
     * Take a list of identifiers that each represent consecutive parts of a namespace, and move into the scope that
     * they represent. For example, given @c std::io::File, the list would contain three identifiers, one for
     * @c std, one for @c io and one for @c File. The scope manager would then move into the @c std scope, then the @c
     * io scope, and finally return the @c File scope. This is used to identify scopes that a namespaced type resides
     * in.
     * @param names The list of identifiers that represent the namespaced parts.
     * @return The scope that the namespaced identifiers represent. If any part of the namespace does not exist,
     * @c nullptr is returned.
     */
    SPP_ATTR_NODISCARD auto get_namespaced_scope(
        std::vector<asts::IdentifierAst const*> const &names) const
        -> Scope*;

    /**
     * For every type discovered up until this point, attach the defined supertypes to them. At this point, all base
     * classes, and @c sup blocks, will have been injected into the symbol table, but possibly not all generic
     * substitutions of some of these type. This is fine because the @c TypeAst semantic analysis will call individual
     * sup scope attachment functions if needed.
     * @param meta The compiler metadata.
     */
    auto attach_all_super_scopes(
        asts::mixins::CompilerMetaData *meta)
        -> void;

    /**
     * The public method to attach all the superscopes to an individual type, represented by its corresponding scope.
     * This is the function called by the type analysis code if the generic substitution is new. For example, of
     * @c Vec[Str] has been discovered after the "attach all" was performed, then this function will be called to attach
     * the superscopes of @c Vec[Str].
     * @param scope The scope representing the type to attach superscopes to.
     * @param meta The compiler metadata.
     */
    auto attach_specific_super_scopes(
        Scope &scope,
        asts::mixins::CompilerMetaData *meta) const
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
    auto attach_specific_super_scopes_impl(
        Scope &scope,
        std::vector<Scope*> &&sup_scopes,
        asts::mixins::CompilerMetaData *meta) const
        -> void;

    /**
     * Once a super scope has been registered against a scope, there are some checks to ensure that this new superscope
     * is semantically compatible with the type. There can be no conflicting @c use or @c cmp statements with existing
     * definitions on existing supertypes of the type.
     * @param cls_sym The type symbol representing the class to check.
     * @param sup_scope The new super scope that has been added to the class.
     */
    static auto check_conflicting_type_or_cmp_statements(
        TypeSymbol const &cls_sym,
        Scope const &sup_scope)
        -> void;
};
