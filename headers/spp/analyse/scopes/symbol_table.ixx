module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbol_table;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS
  template <typename I, typename S>
  class IndividualSymbolTable;

  SPP_EXP_CLS class SymbolTable;
  SPP_EXP_CLS struct NamespaceSymbol;
  SPP_EXP_CLS struct TypeSymbol;
  SPP_EXP_CLS struct VariableSymbol;

  struct TransparentStringHash {
    using is_transparent = void;
    using is_avalanching = void;

    auto operator()(const StrView sv) const noexcept -> std::uint64_t {
      return Hash<StrView>{}(sv);
    }
  };
}

namespace spp::asts {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

SPP_EXP_CLS

template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
  Map<Str, Shared<S>, TransparentStringHash, std::equal_to<>> _Table;

public:
  IndividualSymbolTable();

  ~IndividualSymbolTable();

  /**
   * Force the usage of one of the two explicit copying methods - shallow or deep. This dis-ambiguates exactly what sort
   * of copying is happening between two symbol tables.
   */
  IndividualSymbolTable(IndividualSymbolTable const &that) = delete;
  auto operator=(IndividualSymbolTable const &that) -> IndividualSymbolTable& = delete;

  /**
   * Share the symbols, but in a new map within the new symbol table. Modifying the map of symbols won't affect other
   * tables' actual maps, but the symbols are shjared, so change in all places (all maps containing the changed symbol).
   * @param that The table to share the symbols of.
   */
  auto ShallowCopyFrom(IndividualSymbolTable const &that) -> void;

  /**
   * Copy each symbol individually (should the method on the symbol say that a deep copy is actually required), into the
   * new table. Modifying the symnbols in this table doesn't affect any symbols in other tables.
   * @param that The table to copy the symbols of.
   */
  auto DeepCopyFrom(IndividualSymbolTable const &that) -> void;

  SPP_ATTR_HOT
  auto Add(I const *sym_name, Shared<S> const &sym) -> void;

  auto Rem(I const *sym_name) -> Shared<S>;

  SPP_ATTR_NODISCARD SPP_ATTR_HOT
  auto Get(I const *sym_name) const -> Shared<S>;

  SPP_ATTR_NODISCARD
  auto Has(I const *sym_name) const -> bool;

  SPP_ATTR_NODISCARD
  auto All() const -> Vec<S*>;
};

SPP_EXP_CLS class spp::analyse::scopes::SymbolTable {
public:
  SymbolTable();
  ~SymbolTable();

  SymbolTable(SymbolTable const &that) = delete;
  auto operator=(SymbolTable const &that) -> SymbolTable& = delete;

  /**
   * Use the shallow copy on the individual symbol tables.
   * @param that The table to share the symbols of.
   */
  auto ShallowCopyFrom(SymbolTable const &that) -> void;

  /**
   * Use the deep copy on the individual symbol tables.
   * @param that The table to copy the symbols of.
   */
  auto DeepCopyFrom(SymbolTable const &that) -> void;

  IndividualSymbolTable<asts::IdentifierAst, NamespaceSymbol> NsTbl;
  IndividualSymbolTable<asts::TypeIdentifierAst, TypeSymbol> TypeTbl;
  IndividualSymbolTable<asts::IdentifierAst, VariableSymbol> VarTbl;
};
