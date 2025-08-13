#pragma once

#include <generator>
#include <map>

#include <spp/asts/_fwd.hpp>


namespace spp::analyse::scopes {
    template <typename I, typename S>
    class IndividualSymbolTable;

    class SymbolTable;
    struct Symbol;
}


template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
    std::map<I*, S*> m_table;

public:
    IndividualSymbolTable();

    IndividualSymbolTable(SymbolTable const &);

    auto add(Symbol *sym) -> void;

    auto rem(I *sym_name) -> void;

    auto get(I *sym_name) -> S*;

    auto set(I *sym_name, S *sym) -> void;

    auto has(I *sym_name) -> bool;

    auto all() -> std::generator<S*>;
};


class spp::analyse::scopes::SymbolTable {
public:
    IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> var_tbl;

    IndividualSymbolTable<asts::TypeIdentifierAst, TypeSymbol> type_tbl;

    IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> namespace_tbl;
};
