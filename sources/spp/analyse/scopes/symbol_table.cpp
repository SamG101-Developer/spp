module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbol_table;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::IndividualSymbolTable() :
    _Table() {
}

template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::IndividualSymbolTable(
    IndividualSymbolTable const &that) {
    // Copy constructor from another symbol table.
    _Table = that._Table;
}

template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::~IndividualSymbolTable() = default;

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::operator=(
    IndividualSymbolTable const &that)
    -> IndividualSymbolTable& {
    for (auto const &[k, v] : that._Table) {
        _Table[k] = MakeShared<S>(*v);
    }
    return *this;
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Add(
    Shared<I> const &sym_name,
    Shared<S> const &sym)
    -> void {
    // Add a symbol to the table. Use string_view for the find to avoid a copy.
    const auto sv = sym_name->ToView();
    auto it = _Table.find(sv);
    if (it != _Table.end()) {
        it->second = sym;
    }
    else {
        _Table.emplace(sv, sym);
    }
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Rem(
    Shared<I> const &sym_name) -> Shared<S> {
    // Remove a symbol from the table.
    auto it = _Table.find(sym_name->ToView());
    if (it != _Table.end()) {
        auto sym = it->second;
        _Table.erase(it);
        return sym;
    }
    return nullptr;
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Get(
    Shared<I> const &sym_name) const
    -> Shared<S> {
    // Get a symbol from the table. Use string_view to avoid a string copy per lookup.
    if (sym_name == nullptr) { return nullptr; }
    auto ptr = _Table.find(sym_name->ToView());
    return ptr != _Table.end() ? ptr->second : nullptr;
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Has(
    Shared<I> const &sym_name) const
    -> bool {
    // Check if a symbol exists in the table.
    if (sym_name == nullptr) { return false; }
    auto ptr = _Table.find(sym_name->ToView());
    return ptr != _Table.end();
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::All() const
    -> Vec<Shared<S>> {
    // Generate all symbols in the table.
    return _Table
        | genex::views::vals
        | genex::to<Vec>();
}

spp::analyse::scopes::SymbolTable::SymbolTable() = default;

spp::analyse::scopes::SymbolTable::SymbolTable(
    SymbolTable const &that) :
    NsTbl(that.NsTbl),
    TypeTbl(that.TypeTbl),
    VarTbl(that.VarTbl) {
}

spp::analyse::scopes::SymbolTable::~SymbolTable() = default;

auto spp::analyse::scopes::SymbolTable::operator=(
    SymbolTable const &that)
    -> SymbolTable& {
    // Assignment operator.
    NsTbl = that.NsTbl;
    TypeTbl = that.TypeTbl;
    VarTbl = that.VarTbl;
    return *this;
}

template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::IdentifierAst, spp::analyse::scopes::NamespaceSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::TypeIdentifierAst, spp::analyse::scopes::TypeSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::IdentifierAst, spp::analyse::scopes::VariableSymbol>;
SPP_MOD_END
