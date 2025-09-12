#pragma once
#include <spp/analyse/scopes/scope.hpp>
#include <spp/utils/error_formatter.hpp>


namespace spp::analyse::scopes {
    class ScopeManager;
    class ScopeIterator;
    class ScopeRange;
    struct TypeSymbol;
}


namespace spp::asts::mixins {
    struct CompilerMetaData;
}


class spp::analyse::scopes::ScopeIterator {
    Scope *m_root;
    std::vector<Scope*> m_stack;
    std::size_t m_seen_children;

public:
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;
    using value_type = Scope*;
    using difference_type = std::ptrdiff_t;
    using pointer = Scope**;
    using reference = Scope*&;
    using const_pointer = Scope* const*;
    using const_reference = Scope* const&;

    explicit ScopeIterator(Scope *root = nullptr);
    auto operator*() -> reference;
    auto operator*() const -> const_reference;
    auto operator->() -> pointer;
    auto operator->() const -> const_pointer;
    auto operator++() -> ScopeIterator&;
    auto operator++(int) -> ScopeIterator;
    auto operator==(ScopeIterator const &other) const -> bool;
    auto operator!=(ScopeIterator const &other) const -> bool;
};


class spp::analyse::scopes::ScopeManager {
    friend struct asts::TypeStatementAst;

private:
    ScopeIterator m_it;

public:
    inline static std::map<TypeSymbol*, std::vector<Scope*>> normal_sup_blocks = {};

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
    explicit ScopeManager(std::shared_ptr<Scope> const &global_scope, Scope *current_scope = nullptr);

    auto iter() const -> ScopeRange;

    auto reset(Scope *scope = nullptr, std::optional<ScopeIterator> iterator = std::nullopt) -> void;

    auto create_and_move_into_new_scope(ScopeName name, asts::Ast *ast = nullptr, spp::utils::errors::ErrorFormatter *error_formatter = nullptr) -> Scope*;

    auto move_out_of_current_scope() -> Scope*;

    SPP_NO_ASAN auto move_to_next_scope() -> Scope*;

    auto get_namespaced_scope(std::vector<asts::IdentifierAst const*> const &names) const -> Scope*;

    auto attach_all_super_scopes(asts::mixins::CompilerMetaData *meta) -> void;

    auto attach_specific_super_scopes(Scope &scope, asts::mixins::CompilerMetaData *meta) -> void;

private:
    auto attach_specific_super_scopes_impl(Scope &scope, std::vector<Scope*> &&sup_scopes, asts::mixins::CompilerMetaData *meta) -> void;

    static auto check_conflicting_type_or_cmp_statements(TypeSymbol const &cls_sym, Scope const &sup_scope) -> void;
};


class spp::analyse::scopes::ScopeRange {
    Scope *m_root;

public:
    explicit ScopeRange(Scope *root);
    auto begin() const -> ScopeIterator;
    auto end() const -> ScopeIterator;
};
