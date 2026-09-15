module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbol_table;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, struct NamespaceSymbol);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::scopes, class SymbolTable);
use(spp::analyse::scopes, struct TransparentStringHash);
use(spp::analyse::scopes, template <typename I, typename S> class IndividualSymbolTable);
use(spp::analyse::scopes, template <typename I> struct SymbolTableKeyOf;);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeIdentifierAst);

/// Optimized hashing strategy for the extremely hot paths
/// of symbol table access.
SPP_EXP_CLS struct spp::analyse::scopes::TransparentStringHash {
  using is_transparent = void;
  using is_avalanching = void;

  auto operator()(const StrView sv) const noexcept -> std::uint64_t {
    return Hash<StrView>{}(sv);
  }
};

/// A plain identifier is indexed by its interned id. As the
/// name never mutates after the node is built, the interning
/// will always work.
template <>
struct spp::analyse::scopes::SymbolTableKeyOf<IdentifierAst> {
  using Type = utils::InternedId;
  using Hasher = Hash<utils::InternedId>;
  using Eq = std::equal_to<>;
};

/// A type is more complex, and is indexed by its name and
/// generic arguments, so that "Vec[Str]" and "Vec[U8]" are
/// distinct. Type names can mutate, so no interning.
template <>
struct spp::analyse::scopes::SymbolTableKeyOf<TypeIdentifierAst> {
  using Type = Str;
  using Hasher = TransparentStringHash;
  using Eq = std::equal_to<>;
};

/// An individual symbol table contains one type of symbol; in
/// this case, either the variable, type or namespace symbols.
/// It provides simple mutation methods that the master symbol
/// table hooks into.
SPP_EXP_CLS template <typename I, typename S>
class spp::analyse::scopes::IndividualSymbolTable {
public:
  IndividualSymbolTable();

  ~IndividualSymbolTable();

  /// Force the usage of one of the two explicit copying methods:
  /// shallow or deep. This dis-ambiguates exactly what sort of
  /// copying is happening between two symbol tables.
  IndividualSymbolTable(IndividualSymbolTable const &that) = delete;
  auto operator=(IndividualSymbolTable const &that) -> IndividualSymbolTable& = delete;

  /// Share the symbols, but in a new map within the new symbol
  /// table. Modifying the map of symbols won't affect other
  /// tables' actual maps, but the symbols are shared, so change
  /// in all places (all maps containing the changed symbol).
  auto ShallowCopyFrom(IndividualSymbolTable const &that) -> void;

  /// Copy each symbol individually (should the method on the
  /// symbol say that a deep copy is actually required), into
  /// the new table. Modifying the symbols in this table doesn't
  /// affect any symbols in other tables.
  auto DeepCopyFrom(IndividualSymbolTable const &that) -> void;

  /// Add a symbol into the table, replacing any existing symbol
  /// with the same name.
  SPP_ATTR_HOT auto Add(I const *sym_name, Shared<S> const &sym) -> void;

  /// Remove a symbol from the table, returning it.
  auto Rem(I const *sym_name) -> Shared<S>;

  /// Query the table for a symbol with a matching name. Return
  /// it in raw pointer form to prevent unnecessary copies.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto Get(I const *sym_name) const -> S*;

  /// Get all the symbols in the table unrolled into a vector.
  SPP_ATTR_NODISCARD auto All() const -> Vec<S*>;

private:
  using Key = typename SymbolTableKeyOf<I>::Type;

  /// The actual symbol table.
  Map<Key, Shared<S>, typename SymbolTableKeyOf<I>::Hasher, typename SymbolTableKeyOf<I>::Eq> _Table;
};

/// The combined symbol table holder, that holds all three
/// individual symbol tables. This is what every scope will
/// hold an instance of.
SPP_EXP_CLS class spp::analyse::scopes::SymbolTable {
public:
  SymbolTable();
  ~SymbolTable();

  SymbolTable(SymbolTable const &that) = delete;
  auto operator=(SymbolTable const &that) -> SymbolTable& = delete;

  /// Use the shallow copy on each individual symbol tables.
  auto ShallowCopyFrom(SymbolTable const &that) -> void;

  /// Use the deep copy on each individual symbol tables.
  auto DeepCopyFrom(SymbolTable const &that) -> void;

  IndividualSymbolTable<IdentifierAst, NamespaceSymbol> NsTbl;
  IndividualSymbolTable<TypeIdentifierAst, TypeSymbol> TypeTbl;
  IndividualSymbolTable<IdentifierAst, VariableSymbol> VarTbl;
};
