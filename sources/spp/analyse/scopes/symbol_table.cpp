#include <spp/analyse/scopes/symbol_table.hpp>

#include <genex/views/to.hpp>
#include <genex/views/transform.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#pragma GCC diagnostic ignored "-Wstrict-overflow=5"
#include <nlohmann/json.hpp>
#pragma GCC diagnostic pop


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
spp::analyse::scopes::IndividualSymbolTable<I, S>::operator std::string() const {
    return nlohmann::json(m_table
            | genex::views::transform([](auto const &pair) { return std::make_pair(static_cast<std::string>(*pair.first), static_cast<std::string>(*pair.second)); })
            | genex::views::to<std::map<std::string, std::string>>())
        .dump();
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::add(
    I const &sym_name,
    std::shared_ptr<S> sym)
    -> void {
    // Add a symbol to the table.
    m_table[sym_name] = sym;
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::rem(
    I const &sym_name) -> void {
    // Remove a symbol from the table.
    m_table.erase(sym_name);
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::get(
    I const &sym_name) const
    -> std::shared_ptr<S> {
    // Get a symbol from the table.
    return m_table[sym_name];
}


template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::has(
    I const &sym_name) const
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
