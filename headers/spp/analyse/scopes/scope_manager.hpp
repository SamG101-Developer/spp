#ifndef SCOPE_MANAGER_HPP
#define SCOPE_MANAGER_HPP

#include <map>
#include <stack>
#include <vector>

#include <spp/analyse/scopes/scope.hpp>
#include <spp/utils/error_formatter.hpp>

namespace spp::analyse::scopes {
    class ScopeManager;
    struct ScopeManagerIterator;
    struct TypeSymbol;
}


namespace spp::asts::mixins {
    struct CompilerMetaData;
}


struct spp::analyse::scopes::ScopeManagerIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = Scope;
    using difference_type = std::ptrdiff_t;
    using pointer = Scope*;
    using reference = Scope&;

private:
    std::stack<std::pair<pointer, std::size_t>> m_stack;

public:
    ScopeManagerIterator() = default;

    explicit ScopeManagerIterator(Scope *root);

    auto operator*() const -> reference;

    auto operator->() const -> pointer;

    auto operator++() -> ScopeManagerIterator&;

    auto operator++(int) -> ScopeManagerIterator;

    friend bool operator==(const ScopeManagerIterator &lhs, const ScopeManagerIterator &rhs);

    friend bool operator!=(const ScopeManagerIterator &lhs, const ScopeManagerIterator &rhs);
};


class spp::analyse::scopes::ScopeManager {
public:
    static std::map<TypeSymbol*, std::vector<Scope*>> normal_sup_blocks;

    static std::map<TypeSymbol*, std::vector<Scope*>> generic_sup_blocks;

public:
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

private:
    ScopeManagerIterator m_iterator;

public:
    explicit ScopeManager(std::shared_ptr<Scope> global_scope, Scope *current_scope = nullptr);

    auto begin() -> ScopeManagerIterator;

    auto end() -> ScopeManagerIterator;

    auto reset(Scope *scope = nullptr, std::optional<ScopeManagerIterator> it = std::nullopt) -> void;

    auto create_and_move_into_new_scope(ScopeName name, asts::Ast *ast = nullptr, spp::utils::errors::ErrorFormatter *error_formatter = nullptr) -> Scope*;

    auto move_out_of_current_scope() -> Scope*;

    auto move_to_next_scope() -> Scope*;

    auto get_namespaced_scope(std::vector<asts::IdentifierAst*> const &names) -> Scope*;

    auto attach_all_super_scopes(asts::mixins::CompilerMetaData *meta) -> void;

    auto attach_specific_super_scopes(Scope *scope, asts::mixins::CompilerMetaData *meta) -> void;

private:
    auto attach_specific_super_scopes_impl(Scope *scope, std::vector<Scope*> &&sup_scopes, asts::mixins::CompilerMetaData *meta) -> void;
};


#endif //SCOPE_MANAGER_HPP
