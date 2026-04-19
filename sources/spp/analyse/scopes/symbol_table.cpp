module spp.analyse.scopes;
import spp.asts;
import genex;


spp::analyse::scopes::IndividualSymbolTable::IndividualSymbolTable() = default;


spp::analyse::scopes::IndividualSymbolTable::IndividualSymbolTable(
    IndividualSymbolTable const &that) {
    // Copy constructor from another symbol table.
    m_table = that.m_table;
}


spp::analyse::scopes::IndividualSymbolTable::~IndividualSymbolTable() = default;


auto spp::analyse::scopes::IndividualSymbolTable::operator=(
    IndividualSymbolTable const &)
    -> IndividualSymbolTable& {
    // TODO: Re-Add
    // for (auto const &[k, v] : that.m_table) {
    //     m_table[k] = std::make_shared<S>(*v);
    // }
    return *this;
}


auto spp::analyse::scopes::IndividualSymbolTable::add(
    std::string const &sym_name,
    std::shared_ptr<AbstractSymbol> const &sym)
    -> void {
    // Add a symbol to the table.
    if (has(sym_name)) {
        for (auto it = m_table.begin(); it != m_table.end(); ++it) {
            if (it->first == sym_name) {
                m_table.erase(it);
                break;
            }
        }
    }
    m_table[sym_name] = sym;
}


auto spp::analyse::scopes::IndividualSymbolTable::rem(
    std::string const &sym_name) -> void {
    // Remove a symbol from the table.
    const auto it = m_table.find(sym_name);
    if (it != m_table.end()) { m_table.erase(it); }
}


auto spp::analyse::scopes::IndividualSymbolTable::get(
    std::string const &sym_name) const
    -> AbstractSymbol* {
    // Get a symbol from the table.
    const auto ptr = m_table.find(sym_name);
    return ptr != m_table.end() ? ptr->second.get() : nullptr;
}


auto spp::analyse::scopes::IndividualSymbolTable::has(
    std::string const &sym_name) const
    -> bool {
    // Check if a symbol exists in the table.
    const auto ptr = m_table.find(sym_name);
    return ptr != m_table.end();
}


auto spp::analyse::scopes::IndividualSymbolTable::all() const
    -> std::vector<std::shared_ptr<AbstractSymbol>> {
    // Generate all symbols in the table.
    return m_table | genex::views::vals | genex::to<std::vector>();
}


spp::analyse::scopes::SymbolTable::SymbolTable() = default;


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
