#pragma once

#include <spp/macros.hpp>
#include <spp/pch.hpp>
#include <spp/asts/_fwd.hpp>
#include <spp/utils/ptr_cmp.hpp>

#include <hash_table8.hpp>


/// @cond
namespace spp::analyse::scopes {
    template <typename I, typename S>
    class IndividualSymbolTable;

    class SymbolTable;
    struct Symbol;
    struct NamespaceSymbol;
    struct TypeSymbol;
    struct VariableSymbol;
}

/// @endcond


template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
    emhash8::HashMap<std::shared_ptr<I>, std::shared_ptr<S>, spp::utils::PtrHash<std::shared_ptr<I>>, spp::utils::PtrEq<std::shared_ptr<I>>> m_table;

public:
    IndividualSymbolTable();

    /**
     * Light copy
     * @param that
     */
    IndividualSymbolTable(IndividualSymbolTable const &that);

    ~IndividualSymbolTable();

    /**
     * Deep copy
     * @param that
     * @return
     */
    auto operator=(IndividualSymbolTable const &that) -> IndividualSymbolTable&;

    SPP_ATTR_HOT
    auto add(std::shared_ptr<I> const &sym_name, std::shared_ptr<S> const &sym) -> void;

    auto rem(std::shared_ptr<I> const &sym_name) -> void;

    SPP_ATTR_HOT
    auto get(std::shared_ptr<const I> const &sym_name) const -> std::shared_ptr<S>;

    auto has(std::shared_ptr<I> const &sym_name) const -> bool;

    auto all() const -> std::generator<std::shared_ptr<S>>;
};


class spp::analyse::scopes::SymbolTable {
public:
    SymbolTable();

    /**
     * Light copy
     * @param that
     */
    SymbolTable(SymbolTable const &that);

    ~SymbolTable();

    /**
     * Deep copy
     * @param that
     * @return
     */
    auto operator=(SymbolTable const &that) -> SymbolTable&;

    IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> ns_tbl;

    IndividualSymbolTable<asts::TypeIdentifierAst, TypeSymbol> type_tbl;

    IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> var_tbl;
};
