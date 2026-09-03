module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbol_table;
import spp.utils.interner;
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

namespace spp::analyse::scopes {
  /**
   * How a symbol-name ast reduces to the key its table is indexed by. Only the key's type and its hashing live here;
   * the reduction itself needs the ast definitions, so it sits alongside the table's member definitions.
   */
  template <typename I>
  struct SymbolTableKeyOf;

  /**
   * A plain identifier is indexed by its interned id. @c IdentifierAst::Val never changes once the node is built, so
   * the id is fixed at construction and the table never has to hash a string.
   */
  template <>
  struct SymbolTableKeyOf<asts::IdentifierAst> {
    using Type = utils::InternedId;
    using Hasher = Hash<utils::InternedId>;
    using Eq = std::equal_to<>;
  };

  /**
   * A type is indexed by its name together with its generic arguments, so that @c Vec[Str] and @c Vec[U8] are distinct
   * entries. That key is derived from a mutable subtree rather than fixed at construction, so it stays a string until
   * the generic argument group's mutations are funnelled through an interface that can invalidate a cached id.
   */
  template <>
  struct SymbolTableKeyOf<asts::TypeIdentifierAst> {
    using Type = Str;
    using Hasher = TransparentStringHash;
    using Eq = std::equal_to<>;
  };
}

SPP_EXP_CLS

template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
private:
  using Key = typename SymbolTableKeyOf<I>::Type;

  Map<Key, Shared<S>, typename SymbolTableKeyOf<I>::Hasher, typename SymbolTableKeyOf<I>::Eq> _Table;

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

  /**
   * Look a symbol up, without taking ownership of it. The table owns every symbol it holds for as long as it holds it,
   * so a borrowed pointer is what almost every caller wants; minting a @c Shared costs an atomic pair per lookup, and a
   * lookup that walks a scope chain performs one per scope. The callers that do need ownership call
   * @c Symbol::SharedFromThis on the result.
   * @param sym_name The name to look up.
   * @return The symbol, or @c nullptr if this table does not hold it.
   */
  SPP_ATTR_NODISCARD SPP_ATTR_HOT
  auto Get(I const *sym_name) const -> S*;

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
