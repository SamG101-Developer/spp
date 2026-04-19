module;
#include <spp/macros.hpp>

export module spp.analyse.utils.scope_utils;
import spp.analyse.scopes;
import spp.analyse.scopes.symbols;
import spp.asts;
import std;

namespace spp::analyse::utils::scope_utils {
    SPP_EXP_FUN auto add_var_symbol(
        scopes::Scope &scope,
        std::shared_ptr<scopes::VariableSymbol> const &sym)
        -> void;

    SPP_EXP_FUN auto add_var_symbol_check_conflict(
        scopes::Scope &scope,
        std::shared_ptr<scopes::VariableSymbol> const &sym)
        -> void;

    SPP_EXP_FUN auto add_type_symbol(
        scopes::Scope &scope,
        std::shared_ptr<scopes::TypeSymbol> const &sym)
        -> void;
    // auto add_type_symbol_check_conflict(std::shared_ptr<TypeSymbol> const &sym) -> void;
    // auto add_ns_symbol(std::shared_ptr<NamespaceSymbol> const &sym) -> void;

    // auto rem_var_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name) -> void;
    // auto rem_type_symbol(std::shared_ptr<asts::TypeIdentifierAst> const &sym_name) -> void;
    // auto rem_ns_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name) -> void;
    //
    // SPP_ATTR_NODISCARD auto all_var_symbols(bool exclusive = false, bool sup_scope_search = false) const -> std::vector<std::shared_ptr<VariableSymbol>>;
    // SPP_ATTR_NODISCARD auto all_type_symbols(bool exclusive = false, bool sup_scope_search = false) const -> std::vector<std::shared_ptr<TypeSymbol>>;
    // SPP_ATTR_NODISCARD auto all_ns_symbols(bool exclusive = false, bool = false) const -> std::vector<std::shared_ptr<NamespaceSymbol>>;
    //
    // SPP_ATTR_NODISCARD auto has_var_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> bool;
    // SPP_ATTR_NODISCARD auto has_type_symbol(std::shared_ptr<AbstractAst> const &sym_name, bool exclusive = false) const -> bool;
    // SPP_ATTR_NODISCARD auto has_ns_symbol(std::shared_ptr<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> bool;

    SPP_EXP_FUN SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get_var_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<asts::IdentifierAst> const &sym_name,
        bool exclusive = false,
        bool sup_scope_search = true)
        -> std::shared_ptr<scopes::VariableSymbol>;

    SPP_EXP_FUN SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get_type_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<const TypeAst> const &sym_name,
        bool exclusive = false,
        bool sup_scope_search = true)
        -> std::shared_ptr<scopes::TypeSymbol>;

    SPP_EXP_FUN SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get_ns_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<const asts::IdentifierAst> const &sym_name,
        bool exclusive = false)
        -> std::shared_ptr<NamespaceSymbol>;

    SPP_EXP_FUN SPP_ATTR_NODISCARD auto get_var_symbol_outermost(
        scopes::Scope const &scope,
        asts::Ast const &expr)
        -> std::pair<std::shared_ptr<scopes::VariableSymbol>, scopes::Scope const*>;

    SPP_EXP_FUN auto associated_type_symbol(
        scopes::Scope const &scope)
        -> std::shared_ptr<scopes::TypeSymbol>;

    SPP_EXP_FUN auto associated_ns_symbol(
        scopes::Scope const &scope)
        -> std::shared_ptr<scopes::NamespaceSymbol>;
}
