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
};


class spp::analyse::scopes::Scope {
public:
    ScopeName name;

    Scope *parent;

    std::vector<std::unique_ptr<Scope>> children;

    asts::Ast *ast;

    Symbol *ty_sym;

private:
    SymbolTable m_symbol_table;

    std::vector<Scope*> m_direct_sup_scopes;

    std::vector<Scope*> m_direct_sub_scopes;

    Scope *m_non_generic_scope;

    std::unique_ptr<spp::utils::errors::ErrorFormatter> m_error_formatter;

public:
    Scope(ScopeName name, Scope *parent, asts::Ast *ast = nullptr, std::unique_ptr<spp::utils::errors::ErrorFormatter> error_formatter = nullptr);

    Scope(Scope const &other);

    static auto new_global(compiler::Module const &module) -> std::unique_ptr<Scope>;

    auto get_generic_symbols() -> std::vector<asts::GenericArgumentAst*>;

    auto get_extended_generic_symbols() -> std::vector<asts::GenericArgumentAst*>;

    auto add_symbol(std::unique_ptr<Symbol> sym) -> void;

    auto rem_var_symbol(asts::IdentifierAst const &sym_name) -> void;
    auto rem_type_symbol(asts::TypeAst const &sym_name) -> void;
    auto rem_ns_symbol(asts::IdentifierAst const &sym_name) -> void;

    auto all_symbols(bool exclusive = false, bool sup_scope_search = false) -> std::generator<Symbol*>;
    auto all_var_symbols(bool exclusive = false, bool sup_scope_search = false) -> std::generator<VariableSymbol*>;
    auto all_type_symbols(bool exclusive = false, bool sup_scope_search = false) -> std::generator<TypeSymbol*>;
    auto all_ns_symbols(bool exclusive = false) -> std::generator<NamespaceSymbol*>;

    auto has_var_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) const -> bool;
    auto has_type_symbol(asts::TypeAst const &sym_name, bool exclusive = false) const -> bool;
    auto has_ns_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) const -> bool;

    auto get_var_symbol(asts::IdentifierAst const &sym_name, bool exclusive = false) -> VariableSymbol*;
    auto get_type_symbol(asts::TypeAst const &sym_name, bool exclusive = false, bool ignore_alias = false) -> TypeSymbol*;
    auto get_ns_symbol(asts::IdentifierAst &sym_name, bool exclusive = false) -> NamespaceSymbol*;

    auto get_var_symbol_outermost(asts::Ast const &name) -> std::pair<VariableSymbol*, Scope*>;

    auto depth_difference(Scope *scope) -> std::uint32_t;

    auto final_child_scope() -> Scope*;

    auto ancestors() -> std::vector<Scope*>;

    auto parent_module() -> Scope*;

    auto sup_scopes() -> std::vector<Scope*>;

    auto sup_types() -> std::vector<std::shared_ptr<asts::TypeAst>>;

    auto direct_sup_scopes() -> std::vector<Scope*>;

    auto direct_sup_types() -> std::vector<std::shared_ptr<asts::TypeAst>>;

    auto sub_scopes() -> std::vector<Scope*>;

    auto direct_sub_scopes() -> std::vector<Scope*>;
};


#endif //SCOPE_HPP
