#ifndef SCOPE_MANAGER_HPP
#define SCOPE_MANAGER_HPP

#include <map>
#include <stack>
#include <vector>

#include <spp/analyse/scopes/scope.hpp>
#include <spp/utils/error_formatter.hpp>

#include "scope_manager.hpp"

namespace spp::analyse::scopes {
    class ScopeManager;
    // class ScopeIterator;
    struct TypeSymbol;
}


namespace spp::asts::mixins {
    struct CompilerMetaData;
}


class spp::analyse::scopes::ScopeManager {
    friend struct asts::TypeStatementAst;

public:
    static std::map<TypeSymbol*, std::vector<Scope*>> normal_sup_blocks;

    static std::vector<Scope*> generic_sup_blocks;

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
    std::generator<Scope*> m_generator;
    decltype(std::declval<std::generator<Scope*>>().begin()) m_it;
    decltype(std::declval<std::generator<Scope*>>().end()) m_end;

public:
    explicit ScopeManager(std::shared_ptr<Scope> const &global_scope, Scope *current_scope = nullptr);

    auto iter() const -> std::generator<Scope*>;

    auto reset(Scope *scope = nullptr, std::optional<std::generator<Scope*>> gen = std::nullopt) -> void;

    auto create_and_move_into_new_scope(ScopeName name, asts::Ast *ast = nullptr, std::unique_ptr<spp::utils::errors::ErrorFormatter> &&error_formatter = nullptr) -> Scope*;

    auto move_out_of_current_scope() -> Scope*;

    auto move_to_next_scope() -> Scope*;

    auto get_namespaced_scope(std::vector<asts::IdentifierAst*> const &names) const -> Scope*;

    auto attach_all_super_scopes(asts::mixins::CompilerMetaData *meta) -> void;

    auto attach_specific_super_scopes(Scope &scope, asts::mixins::CompilerMetaData *meta) const -> void;

private:
    auto attach_specific_super_scopes_impl(Scope &scope, std::vector<Scope*> &&sup_scopes, asts::mixins::CompilerMetaData *meta) const -> void;

    static auto check_conflicting_type_or_cmp_statements(TypeSymbol const &cls_sym, Scope const &sup_scope) -> void;
};


#endif //SCOPE_MANAGER_HPP
