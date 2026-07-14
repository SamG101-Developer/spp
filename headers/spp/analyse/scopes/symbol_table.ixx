module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbol_table;
import spp.utils.ptr;
import spp.utils.types;
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

        auto operator()(const StrView sv) const noexcept -> std::uint64_t {
            return ankerl::unordered_dense::hash<StrView>{}(sv);
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
    ankerl::unordered_dense::map<Str, Unique<S>, TransparentStringHash, std::equal_to<>> _Table;

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
    auto Add(I *sym_name, Unique<S> &&sym) -> void;

    auto Rem(I const *sym_name) -> Unique<S>;

    SPP_ATTR_NODISCARD SPP_ATTR_HOT
    auto Get(I const *sym_name) const -> S*;

    SPP_ATTR_NODISCARD
    auto Has(I *sym_name) const -> bool;

    SPP_ATTR_NODISCARD
    auto All() const -> Vec<S*>;
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

    IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> NsTbl;
    IndividualSymbolTable<asts::TypeIdentifierAst, TypeSymbol> TypeTbl;
    IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> VarTbl;
};
