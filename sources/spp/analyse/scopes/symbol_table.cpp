#include <spp/analyse/scopes/symbol_table.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


auto spp::analyse::scopes::SymNameCmp<std::shared_ptr<spp::asts::IdentifierAst>>::operator()(
    std::shared_ptr<asts::IdentifierAst> const &lhs,
    std::shared_ptr<asts::IdentifierAst> const &rhs) const
    -> bool {
    return *lhs < *rhs;
}


auto spp::analyse::scopes::SymNameCmp<std::shared_ptr<spp::asts::TypeIdentifierAst>>::operator()(
    std::shared_ptr<asts::TypeIdentifierAst> const &lhs,
    std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const
    -> bool {
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
    std::shared_ptr<I> const &sym_name,
    std::shared_ptr<S> const &sym)
    -> void {
    // Add a symbol to the table.
    if (has(asts::ast_clone(sym_name))) {
        for (auto it = m_table.begin(); it != m_table.end(); ++it) {
            if (*(it->first) == *sym_name) {
                m_table.erase(it);
                break;
            }
        }
    }
    m_table.insert({asts::ast_clone(sym_name), sym});
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::rem(
    std::shared_ptr<I> const &sym_name) -> void {
    // Remove a symbol from the table.
    m_table.erase(m_table.find(sym_name));
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::get(
    std::shared_ptr<const I> const &sym_name) const
    -> std::shared_ptr<S> {
    // Get a symbol from the table.
    if (sym_name == nullptr) { return nullptr; }
    for (auto const &[k, v] : m_table) {
        if (*k == *sym_name) {
            return v;
        }
    }
    return nullptr;
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::has(
    std::shared_ptr<I> const &sym_name) const
    -> bool {
    // Check if a symbol exists in the table.
    if (sym_name == nullptr) { return false; }
    for (auto const &[k, _] : m_table) {
        if (*k == *sym_name) {
            return true;
        }
    }
    return false;
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
template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::TypeIdentifierAst, spp::analyse::scopes::TypeSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<spp::asts::IdentifierAst, spp::analyse::scopes::VariableSymbol>;
