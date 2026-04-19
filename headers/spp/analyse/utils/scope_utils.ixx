module;
#include <spp/macros.hpp>

export module spp.analyse.utils.scope_utils;
import spp.abstract;
import spp.analyse.scopes;
import spp.analyse.scopes.symbols;
import spp.asts;
import spp.codegen.llvm_ctx;
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

    SPP_EXP_FUN auto add_type_symbol_check_conflict(
        scopes::Scope &scope,
        std::shared_ptr<scopes::TypeSymbol> const &sym)
        -> void;

    SPP_EXP_FUN auto add_ns_symbol(
        scopes::Scope &scope,
        std::shared_ptr<scopes::NamespaceSymbol> const &sym)
        -> void;

    auto rem_var_symbol(
        scopes::Scope &scope,
        std::shared_ptr<asts::IdentifierAst> const &sym_name)
        -> void;

    auto rem_type_symbol(
        scopes::Scope &scope,
        std::shared_ptr<asts::TypeIdentifierAst> const &sym_name)
        -> void;

    auto rem_ns_symbol(
        scopes::Scope &scope,
        std::shared_ptr<asts::IdentifierAst> const &sym_name)
        -> void;

    SPP_ATTR_NODISCARD auto all_var_symbols(
        scopes::Scope const &scope,
        bool exclusive = false,
        bool sup_scope_search = false)
        -> std::vector<scopes::VariableSymbol*>;

    SPP_ATTR_NODISCARD auto all_type_symbols(
        scopes::Scope const &scope,
        bool exclusive = false,
        bool sup_scope_search = false)
        -> std::vector<scopes::TypeSymbol*>;

    SPP_ATTR_NODISCARD auto all_ns_symbols(
        scopes::Scope const &scope,
        bool exclusive = false,
        bool = false)
        -> std::vector<scopes::NamespaceSymbol*>;

    SPP_ATTR_NODISCARD auto has_var_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<asts::IdentifierAst> const &sym_name,
        bool exclusive = false)
        -> bool;

    SPP_ATTR_NODISCARD auto has_type_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<AbstractAst> const &sym_name,
        bool exclusive = false)
        -> bool;

    SPP_ATTR_NODISCARD auto has_ns_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<asts::IdentifierAst> const &sym_name,
        bool exclusive = false)
        -> bool;

    SPP_EXP_FUN SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get_var_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<asts::IdentifierAst> const &sym_name,
        bool exclusive = false,
        bool sup_scope_search = true)
        -> std::shared_ptr<scopes::VariableSymbol>;

    SPP_EXP_FUN SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get_type_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<const asts::TypeAst> const &sym_name,
        bool exclusive = false,
        bool sup_scope_search = true)
        -> std::shared_ptr<scopes::TypeSymbol>;

    SPP_EXP_FUN SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get_ns_symbol(
        scopes::Scope const &scope,
        std::shared_ptr<const asts::IdentifierAst> const &sym_name,
        bool exclusive = false)
        -> std::shared_ptr<scopes::NamespaceSymbol>;

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

    SPP_EXP_FUN SPP_ATTR_NODISCARD auto get_scope_generics(
        scopes::Scope const &scope)
        -> std::vector<std::unique_ptr<asts::GenericArgumentAst>>;

    SPP_EXP_FUN SPP_ATTR_NODISCARD auto get_scope_extended_generic_symbols(
        scopes::Scope const &scope,
        std::vector<asts::GenericArgumentAst*> const &generics,
        std::shared_ptr<asts::TypeAst> const &ignore = nullptr)
        -> std::vector<std::shared_ptr<scopes::Symbol>>;

    /**
     * Search all "sup" scopes of an existing scope (this will be a type scope), for a variable
     * symbol with a name that matches "name". This is used when looking for the a constant defined
     * with "cmp" within a sup-block of a type.
     * @param scope The starting scope to search from.
     * @param name The name of the variable symbol to search for.
     * @return The found variable symbol, or nullptr if not found.
     */
    auto search_sup_scopes_for_var(
        scopes::Scope const &scope,
        std::shared_ptr<asts::IdentifierAst> const &name)
        -> std::shared_ptr<scopes::VariableSymbol>;

    /**
     * Search all "sup" scopes of an existing scope (this will be a type scope), for a type
     * symbol with a name that matches "name". This is used when looking for a type defined with a
     * "type" statement within a sup-block of a type.
     * @param scope The starting scope to search from.
     * @param name The name of the type symbol to search for.
     * @return The found type symbol, or nullptr if not found.
     */
    auto search_sup_scopes_for_type(
        scopes::Scope const &scope,
        std::shared_ptr<const asts::TypeAst> const &name)
        -> std::shared_ptr<scopes::TypeSymbol>;

    /**
     * Given a scope and a fully qualified type, this function moves through the namespace parts of
     * the type, moving into the next namespace scopes. Finally, it will arrive at the scope for the
     * innermost (rightmost) namespace part, and the compeltely unqualified type name. For example,
     * @c scope:random_scope and @c std::string::Str becomes @c scope:string and @c Str.
     * @param scope The starting scope to search from.
     * @param fq_type The fully qualified type to shift the scope for.
     * @return A pair of the shifted scope and the unqualified type identifier.
     */
    auto shift_scope_for_namespaced_type(
        scopes::Scope const &scope,
        asts::TypeAst const &fq_type)
        -> std::pair<const scopes::Scope*, std::shared_ptr<const asts::TypeIdentifierAst>>;

    auto attach_llvm_type_info(
        asts::ModulePrototypeAst const &mod,
        codegen::LlvmCtx *ctx)
        -> void;
}
