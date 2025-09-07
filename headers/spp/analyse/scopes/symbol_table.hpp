#pragma once

#include <generator>
#include <map>

#include <spp/asts/_fwd.hpp>

#include <tsl/robin_map.h>


namespace spp::analyse::scopes {
    template <typename I, typename S>
    class IndividualSymbolTable;

    class SymbolTable;
    struct Symbol;
    struct NamespaceSymbol;
    struct TypeSymbol;
    struct VariableSymbol;

    template <typename T>
    struct SymNameCmp;
}


template <typename T>
struct spp::analyse::scopes::SymNameCmp {
    auto operator()(T const *lhs, T const *rhs) const -> bool;
};


template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
    std::map<I const*, std::shared_ptr<S>, SymNameCmp<I>> m_table;

public:
    IndividualSymbolTable();

    IndividualSymbolTable(IndividualSymbolTable const &that);

    ~IndividualSymbolTable();

    auto operator=(IndividualSymbolTable const &that) -> IndividualSymbolTable&;

    // explicit operator std::string() const;

    auto add(I const *sym_name, std::shared_ptr<S> sym) -> void;

    auto rem(I const *sym_name) -> void;

    auto get(I const *sym_name) const -> std::shared_ptr<S>;

    auto has(I const *sym_name) const -> bool;

    auto all() const -> std::generator<std::shared_ptr<S>>;
};


class spp::analyse::scopes::SymbolTable {
public:
    SymbolTable();

    SymbolTable(SymbolTable const &that);

    ~SymbolTable();

    auto operator=(SymbolTable const &that) -> SymbolTable&;

    IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> ns_tbl;

    IndividualSymbolTable<asts::TypeAst, TypeSymbol> type_tbl;

    IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> var_tbl;
};
