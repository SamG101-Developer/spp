#pragma once
#include <spp/macros.hpp>
#include <spp/pch.hpp>
#include <spp/analyse/scopes/scope_block_name.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/scopes/symbol_table.hpp>
#include <spp/asts/_fwd.hpp>
#include <spp/utils/error_formatter.hpp>


/// @cond
namespace spp::analyse::scopes {
    class Scope;
    class ScopeManager;
    using ScopeName = std::variant<
        std::shared_ptr<asts::IdentifierAst>,
        std::shared_ptr<asts::TypeIdentifierAst>,
        ScopeBlockName>;
}

namespace spp::asts::mixins {
    struct CompilerMetaData;
}

namespace spp::compiler {
    struct Module;
}

/// @endcond


class spp::analyse::scopes::Scope {
    friend struct asts::PostfixExpressionOperatorRuntimeMemberAccessAst;
    friend struct asts::PostfixExpressionOperatorStaticMemberAccessAst;
    friend struct asts::TypePostfixExpressionAst;
    friend class ScopeManager;

public:
    /**
     * The name of the scope. This will be either an @c IdentifierAst* (functions, modules), an @c TypeIdentifierAst*
     * (classes), or a @c ScopeBlockName (blocks: @c case, @c loop, etc). It is stored in a @c std::variant to allow for
     * easy type-safe access to the underlying type.
     */
    ScopeName name;

    /**
     * The parent scope. It is a raw pointer as the parents "own" their children, and the children do not own their
     * parents. The parent will be @c nullptr for the global scope.
     */
    Scope *parent;

    /**
     * The child scopes. These are owned by the parent scope, and are stored as @c std::unique_ptr to ensure proper
     * memory management. This allows for an easy traversal of the scope hierarchy.
     */
    std::vector<std::unique_ptr<Scope>> children;

    /**
     * Non child scopes that need an owner (currently from from generic substituted types from non-top-level (aliased)
     * type scopes).
     * @todo: Relocate these scopes at some point.
     */
    std::vector<std::unique_ptr<Scope>> temp_scopes;

    /**
     * Top level scopes register their AST with the scope. This is useful for error reporting, as it allows for easy
     * access to the AST node that the scope represents. This will be @c nullptr for non-top level scopes. Typically,
     * the AST will need to be cast back to its original type.
     */
    asts::Ast *ast;

    /**
     * The (potential) type symbol that represents this scope. This will be @c nullptr for non-type scopes (eg
     * functions, modules, blocks).
     */
    std::shared_ptr<TypeSymbol> ty_sym;

    /**
     * The (potential) namespace symbol that represents this scope. This will be @c nullptr for non-namespace scopes
     * (eg functions, classes, blocks). Note that a namespace is a module.
     */
    std::shared_ptr<NamespaceSymbol> ns_sym;

    /**
     * The collection of symbol tables (type table, variable table, namespace table) that belong to this scope.
     * Individual accessors are created for each table, but the tables are consolidated into a wrapper object for ease
     * of management.
     */
    SymbolTable table;

    /**
     * The scope representing the non-generic version of this scope. If this scope isn't a generic substitution, then
     * the non-generic scope is the scope itself. For @c Vec[Str], the non-generic scope is @c Vec.
     */
    Scope *non_generic_scope;

private:
    std::vector<Scope*> m_direct_sup_scopes;

    std::vector<Scope*> m_direct_sub_scopes;

    spp::utils::errors::ErrorFormatter *m_error_formatter;

public:
    Scope(ScopeName name, Scope *parent, asts::Ast *ast = nullptr, spp::utils::errors::ErrorFormatter *error_formatter = nullptr);

    Scope(Scope const &other);

    static auto new_global(compiler::Module const &module) -> std::unique_ptr<Scope>;

    static auto search_sup_scopes_for_var(Scope const &scope, std::shared_ptr<asts::IdentifierAst> const &name) -> std::shared_ptr<VariableSymbol>;
    static auto search_sup_scopes_for_type(Scope const &scope, std::shared_ptr<const asts::TypeAst> const &name, bool ignore_alias) -> std::shared_ptr<TypeSymbol>;
    static auto shift_scope_for_namespaced_type(Scope const &scope, asts::TypeAst const &fq_type) -> std::pair<const Scope*, std::shared_ptr<const asts::TypeIdentifierAst>>;

    auto get_error_formatter() const -> spp::utils::errors::ErrorFormatter*;

    auto get_generics() const -> std::vector<std::unique_ptr<asts::GenericArgumentAst>>;

    auto get_extended_generic_symbols(std::vector<asts::GenericArgumentAst*> generics) -> std::vector<std::shared_ptr<Symbol>>;

    auto add_var_symbol(std::shared_ptr<VariableSymbol> const &sym) -> void;
    auto add_type_symbol(std::shared_ptr<TypeSymbol> const &sym) -> void;
    auto add_ns_symbol(std::shared_ptr<NamespaceSymbol> const &sym) -> void;

    auto rem_var_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name) -> void;
    auto rem_type_symbol(std::shared_ptr<asts::TypeIdentifierAst> const &sym_name) -> void;
    auto rem_ns_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name) -> void;

    auto all_var_symbols(bool exclusive = false, bool sup_scope_search = false) const -> std::generator<std::shared_ptr<VariableSymbol>>;
    auto all_type_symbols(bool exclusive = false, bool sup_scope_search = false) const -> std::generator<std::shared_ptr<TypeSymbol>>;
    auto all_ns_symbols(bool exclusive = false, bool = false) const -> std::generator<std::shared_ptr<NamespaceSymbol>>;

    auto has_var_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> bool;
    auto has_type_symbol(std::shared_ptr<asts::TypeAst> const &sym_name, bool exclusive = false) const -> bool;
    auto has_ns_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> bool;

    SPP_ATTR_HOT auto get_var_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> std::shared_ptr<VariableSymbol>;
    SPP_ATTR_HOT auto get_type_symbol(std::shared_ptr<const asts::TypeAst> const &sym_name, bool exclusive = false, bool ignore_alias = false) const -> std::shared_ptr<TypeSymbol>;
    SPP_ATTR_HOT auto get_ns_symbol(std::shared_ptr<const asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> std::shared_ptr<NamespaceSymbol>;

    auto get_var_symbol_outermost(asts::Ast const &expr) const -> std::pair<std::shared_ptr<VariableSymbol>, Scope const*>;

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

    auto convert_postfix_to_nested_scope(asts::ExpressionAst const *postfix_ast) const -> Scope const*;

    auto print_scope_tree() const -> std::string;
};
