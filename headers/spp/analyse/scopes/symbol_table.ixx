module;
#include <spp/macros.hpp>

export module spp.analyse.scopes:symbol_table;
import spp.abstract;
import ankerl;
import std;


namespace spp::analyse::scopes {
    SPP_EXP_CLS class IndividualSymbolTable;
    SPP_EXP_CLS class SymbolTable;
}


SPP_EXP_CLS class spp::analyse::scopes::IndividualSymbolTable {
private:
    ankerl::unordered_dense::map<std::string, std::shared_ptr<AbstractSymbol>> m_table;

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

    SPP_ATTR_HOT auto add(std::string const &sym_name, std::shared_ptr<AbstractSymbol> const &sym) -> void;

    auto rem(std::string const &sym_name) -> void;

    SPP_ATTR_NODISCARD SPP_ATTR_HOT auto get(std::string const &sym_name) const -> AbstractSymbol*;

    SPP_ATTR_NODISCARD auto has(std::string const &sym_name) const -> bool;

    SPP_ATTR_NODISCARD auto all() const -> std::vector<AbstractSymbol*>;
};


SPP_EXP_CLS class spp::analyse::scopes::SymbolTable {
public:
    SymbolTable();

    /**
     * Shallow copy
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

    IndividualSymbolTable ns_tbl;
    IndividualSymbolTable type_tbl;
    IndividualSymbolTable var_tbl;
};
