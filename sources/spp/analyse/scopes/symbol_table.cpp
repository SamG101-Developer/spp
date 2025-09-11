#include <spp/analyse/scopes/symbol_table.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_ast.hpp>


template <typename T>
auto spp::analyse::scopes::SymNameCmp<T>::operator()(T const *lhs, T const *rhs) const -> bool {
    return *lhs < *rhs;
}


template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::IndividualSymbolTable() :
    m_table() {
}


template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::IndividualSymbolTable(
    IndividualSymbolTable const &that) {
    // Copy constructor from another symbol table.
    m_table = that.m_table;
}


template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::~IndividualSymbolTable() = default;


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::operator=(
    IndividualSymbolTable const &that)
    -> IndividualSymbolTable& {
    // Assignment operator.
    m_table = that.m_table;
    return *this;
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::add(
    I const *sym_name,
    std::shared_ptr<S> const & sym)
    -> void {
    // Add a symbol to the table.
    m_table[sym_name] = sym;
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::rem(
    I const *sym_name) -> void {
    // Remove a symbol from the table.
    m_table.erase(m_table.find(sym_name));
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::get(
    I const *sym_name) const
    -> std::shared_ptr<S> {
    // Get a symbol from the table.
    if (sym_name == nullptr or not has(sym_name)) { return nullptr; }
    return m_table.at(sym_name);
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::has(
    I const *sym_name) const
    -> bool {
    // Check if a symbol exists in the table.
    return m_table.contains(sym_name);
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::all() const
    -> std::generator<std::shared_ptr<S>> {
    // Generate all symbols in the table.
    for (auto const &[_, sym] : m_table) {
        co_yield sym;
    }
}


spp::analyse::scopes::SymbolTable::SymbolTable() :
    ns_tbl(),
    type_tbl(),
    var_tbl() {
}


spp::analyse::scopes::SymbolTable::SymbolTable(
    SymbolTable const &that) :
    ns_tbl(that.ns_tbl),
    type_tbl(that.type_tbl),
    var_tbl(that.var_tbl) {
}


spp::analyse::scopes::SymbolTable::~SymbolTable() = default;


auto spp::analyse::scopes::SymbolTable::operator=(
    SymbolTable const &that)
    -> SymbolTable& {
    // Assignment operator.
    ns_tbl = that.ns_tbl;
    type_tbl = that.type_tbl;
    var_tbl = that.var_tbl;
    return *this;
}


template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::IdentifierAst, spp::analyse::scopes::NamespaceSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::TypeAst, spp::analyse::scopes::TypeSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::IdentifierAst, spp::analyse::scopes::VariableSymbol>;
