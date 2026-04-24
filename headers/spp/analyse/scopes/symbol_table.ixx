module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbol_table;
import spp.utils.ptr;
import ankerl.unordered_dense;
import std;


namespace spp::analyse::scopes {
    SPP_EXP_CLS template <typename I, typename S>
    class IndividualSymbolTable;

    SPP_EXP_CLS class SymbolTable;
    SPP_EXP_CLS struct NamespaceSymbol;
    SPP_EXP_CLS struct TypeSymbol;
    SPP_EXP_CLS struct VariableSymbol;

    struct TransparentStringHash {
        using is_transparent = void;
        using is_avalanching = void;
        auto operator()(const std::string_view sv) const noexcept -> std::uint64_t {
            return ankerl::unordered_dense::hash<std::string_view>{}(sv);
        }
    };
}

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


SPP_EXP_CLS template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
    ankerl::unordered_dense::map<std::string, std::shared_ptr<S>, TransparentStringHash, std::equal_to<>> m_table;

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

    SPP_ATTR_HOT
    auto add(std::shared_ptr<I> const &sym_name, std::shared_ptr<S> const &sym) -> void;

    auto rem(std::shared_ptr<I> const &sym_name) -> void;

    SPP_ATTR_NODISCARD SPP_ATTR_HOT
    auto get(std::shared_ptr<I> const &sym_name) const -> std::shared_ptr<S>;

    SPP_ATTR_NODISCARD
    auto has(std::shared_ptr<I> const &sym_name) const -> bool;

    SPP_ATTR_NODISCARD
    auto all() const -> std::vector<std::shared_ptr<S>>;
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

    IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> ns_tbl;
    IndividualSymbolTable<asts::TypeIdentifierAst, TypeSymbol> type_tbl;
    IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> var_tbl;
};
