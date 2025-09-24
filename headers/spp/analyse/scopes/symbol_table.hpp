#pragma once

#include <spp/asts/_fwd.hpp>
#include <spp/macros.hpp>
#include <spp/pch.hpp>

#include <tsl/robin_map.h>


/// @cond
namespace spp::analyse::scopes {
    template <typename I, typename S>
    class IndividualSymbolTable;

    class SymbolTable;
    struct Symbol;
    struct NamespaceSymbol;
    struct TypeSymbol;
    struct VariableSymbol;

    template <typename>
    struct SymNameCmp;
}
/// @endcond


template <typename>
struct spp::analyse::scopes::SymNameCmp {
};


template <>
struct spp::analyse::scopes::SymNameCmp<std::shared_ptr<spp::asts::IdentifierAst>> {
    auto operator()(std::shared_ptr<asts::IdentifierAst> const &lhs, std::shared_ptr<asts::IdentifierAst> const &rhs) const -> bool;
};


template <>
struct spp::analyse::scopes::SymNameCmp<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &lhs, std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const -> bool;
};


template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
    std::map<std::shared_ptr<I>, std::shared_ptr<S>, SymNameCmp<std::shared_ptr<I>>> m_table;

public:
    IndividualSymbolTable();

    IndividualSymbolTable(IndividualSymbolTable const &that);

    ~IndividualSymbolTable();

    auto operator=(IndividualSymbolTable const &that) -> IndividualSymbolTable&;

    auto add(std::shared_ptr<I> const &sym_name, std::shared_ptr<S> const &sym) -> void;

    auto rem(std::shared_ptr<I> const &sym_name) -> void;

    auto get(std::shared_ptr<const I> const &sym_name) const -> std::shared_ptr<S>;

    auto has(std::shared_ptr<I> const &sym_name) const -> bool;

    auto all() const -> std::generator<std::shared_ptr<S>>;
};


class spp::analyse::scopes::SymbolTable {
public:
    SymbolTable();

    SymbolTable(SymbolTable const &that);

    ~SymbolTable();

    auto operator=(SymbolTable const &that) -> SymbolTable&;

    IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> ns_tbl;

    IndividualSymbolTable<asts::TypeIdentifierAst, TypeSymbol> type_tbl;

    IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> var_tbl;
};
