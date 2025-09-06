#ifndef SCOPE_HPP
#define SCOPE_HPP

#include <generator>
#include <variant>

#include <spp/asts/_fwd.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/scopes/symbol_table.hpp>
#include <spp/utils/error_formatter.hpp>


namespace spp::analyse::scopes {
    struct ScopeBlockName;
    class Scope;
    using ScopeName = std::variant<asts::IdentifierAst*, asts::TypeIdentifierAst*, ScopeBlockName>;
}

namespace spp::compiler {
    struct Module;
}


struct spp::analyse::scopes::ScopeBlockName {
    std::string name;
    explicit ScopeBlockName(std::string name);
    ScopeBlockName(ScopeBlockName const &) = default;
    ScopeBlockName(ScopeBlockName &&) noexcept = default;
};


class spp::analyse::scopes::Scope {
    friend struct asts::PostfixExpressionOperatorRuntimeMemberAccessAst;
    friend struct asts::PostfixExpressionOperatorStaticMemberAccessAst;
    friend struct asts::TypePostfixExpressionAst;
    friend class ScopeManager;

public:
    ScopeName name;

    Scope *parent;

    std::vector<std::unique_ptr<Scope>> children;

    asts::Ast *ast;

    std::shared_ptr<TypeSymbol> ty_sym;

    std::shared_ptr<NamespaceSymbol> ns_sym;

private:
    SymbolTable m_sym_table;

    std::vector<Scope*> m_direct_sup_scopes;

    std::vector<Scope*> m_direct_sub_scopes;

    Scope *m_non_generic_scope;

    std::unique_ptr<spp::utils::errors::ErrorFormatter> m_error_formatter;

private:
    template <typename S, typename T, typename This>
    auto all_symbols_impl(T const &map, This const &func, bool exclusive, bool sup_scope_search) const -> std::generator<S*>;

public:
    Scope(ScopeName name, Scope *parent, asts::Ast *ast = nullptr, std::unique_ptr<spp::utils::errors::ErrorFormatter> &&error_formatter = nullptr);

    Scope(Scope const &other);

    static auto new_global(compiler::Module const &module) -> std::unique_ptr<Scope>;

    static auto search_sup_scopes_for_var(Scope const &scope, asts::IdentifierAst const &name) -> VariableSymbol*;
    static auto search_sup_scopes_for_type(Scope const &scope, asts::TypeAst const &name, bool ignore_alias) -> TypeSymbol*;
    static auto shift_scope_for_namespaced_type(Scope const &scope, asts::TypeAst const &fq_type) -> std::pair<const Scope*, std::shared_ptr<const asts::TypeIdentifierAst>>;

    auto get_error_formatter() const -> spp::utils::errors::ErrorFormatter*;

    auto get_generics() -> std::vector<std::unique_ptr<asts::GenericArgumentAst>>;

    auto get_extended_generic_symbols(std::vector<asts::GenericArgumentAst*> generics) -> std::vector<Symbol*>;

    auto add_var_symbol(std::shared_ptr<VariableSymbol> sym) -> void;
    auto add_type_symbol(std::shared_ptr<TypeSymbol> sym) -> void;
    auto add_ns_symbol(std::shared_ptr<NamespaceSymbol> sym) -> void;

    auto rem_var_symbol(asts::IdentifierAst const &sym_name) -> void;
    auto rem_type_symbol(asts::TypeAst const &sym_name) -> void;
    auto rem_ns_symbol(asts::IdentifierAst const &sym_name) -> void;

    auto all_var_symbols(bool exclusive = false, bool sup_scope_search = false) const -> std::generator<VariableSymbol*>;
    auto all_type_symbols(bool exclusive = false, bool sup_scope_search = false) const -> std::generator<TypeSymbol*>;
    auto all_ns_symbols(bool exclusive = false, bool = false) const -> std::generator<NamespaceSymbol*>;

    auto has_var_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) const -> bool;
    auto has_type_symbol(asts::TypeAst const &sym_name, bool exclusive = false) const -> bool;
    auto has_ns_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) const -> bool;

    auto get_var_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) const -> VariableSymbol*;
    auto get_type_symbol(asts::TypeAst const &sym_name, bool exclusive = false, bool ignore_alias = false) const -> TypeSymbol*;
    auto get_ns_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) const -> NamespaceSymbol*;

    auto get_var_symbol_outermost(asts::Ast const &expr) const -> std::pair<VariableSymbol*, Scope const*>;

    auto depth_difference(const Scope *scope) const -> ssize_t;

    auto final_child_scope() const -> Scope const*;

    auto ancestors() const -> std::vector<Scope const*>;

    auto parent_module() const -> Scope const*;

    auto sup_scopes() const -> std::vector<Scope*>;

    auto sup_types() const -> std::vector<std::shared_ptr<asts::TypeAst>>;

    auto direct_sup_scopes() const -> std::vector<Scope*>;

    auto direct_sup_types() const -> std::vector<std::shared_ptr<asts::TypeAst>>;

    auto sub_scopes() -> std::vector<Scope*>;

    auto direct_sub_scopes() -> std::vector<Scope*>;
};


#endif //SCOPE_HPP
