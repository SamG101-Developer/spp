#pragma once

#include <generator>
#include <map>

#include <spp/asts/_fwd.hpp>


namespace spp::analyse::scopes {
    template <typename I, typename S>
    class IndividualSymbolTable;

    class SymbolTable;
    struct Symbol;
    struct NamespaceSymbol;
    struct TypeSymbol;
    struct VariableSymbol;
}


template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
    std::map<I*, std::unique_ptr<S>> m_table;

public:
    IndividualSymbolTable();

    IndividualSymbolTable(SymbolTable const &);

    auto add(std::shared_ptr<Symbol> sym) -> void;

    auto rem(I const &sym_name) -> void;

    auto get(I const &sym_name) -> std::shared_ptr<S>;

    auto set(I const &sym_name, std::shared_ptr<S> sym) -> void;

    auto has(I const &sym_name) -> bool;

    auto all() -> std::generator<std::shared_ptr<S>>;
};


class spp::analyse::scopes::SymbolTable {
public:
    IndividualSymbolTable<asts::IdentifierAst, std::shared_ptr<NamespaceSymbol>> namespace_tbl;

    IndividualSymbolTable<asts::TypeIdentifierAst, std::shared_ptr<TypeSymbol>> type_tbl;

    IndividualSymbolTable<asts::IdentifierAst, std::shared_ptr<VariableSymbol>> var_tbl;
};
