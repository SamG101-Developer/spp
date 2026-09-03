module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbol_table;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
namespace spp::analyse::scopes {
  namespace {
    /**
     * Reduce a name ast to the key its table is indexed by. An identifier reduces to its interned id, which is fixed when
     * the node is built; a type reduces to its name and generic arguments, which is only settled once the node has been
     * analysed. Both are looked up through the table's heterogeneous key, so neither allocates.
     */
    SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto SymbolKey(
      asts::IdentifierAst const *const sym_name) noexcept -> spp::utils::InternedId {
      return sym_name->NameId();
    }

    SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto SymbolKey(
      asts::TypeIdentifierAst const *const sym_name) -> spp::StrView {
      return sym_name->ToView();
    }
  }
}

template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::IndividualSymbolTable() :
  _Table() {
}

template <typename I, typename S>
spp::analyse::scopes::IndividualSymbolTable<I, S>::~IndividualSymbolTable() = default;

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::ShallowCopyFrom(
  IndividualSymbolTable const &that)
  -> void {
  // Copying the map copies the shared pointers, so both
  // tables name the same symbol objects.
  _Table = that._Table;
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::DeepCopyFrom(
  IndividualSymbolTable const &that)
  -> void {
  // Copy each symbol that a substitution goes on to rewrite,
  // so that neither table can be changed through the other,
  // and share the rest - most of a template's symbols mean
  // the same thing from inside every instantiation of it.
  _Table = {};
  _Table.reserve(that._Table.size());
  for (auto const &[k, v] : that._Table) {
    _Table.emplace(k, v->NeedsDeepCopy() ? MakeShared<S>(*v) : v);
  }
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Add(
  I const *sym_name,
  Shared<S> const &sym)
  -> void {
  // Add a symbol to the table, keyed heterogeneously so
  // that nothing is materialised for the find.
  const auto key = SymbolKey(sym_name);
  auto it = _Table.find(key);
  if (it != _Table.end()) {
    it->second = sym;
  }
  else {
    _Table.emplace(key, sym);
  }
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Rem(
  I const *sym_name)
  -> Shared<S> {
  // Remove a symbol from the table.
  auto it = _Table.find(SymbolKey(sym_name));
  if (it != _Table.end()) {
    auto sym = it->second;
    _Table.erase(it);
    return sym;
  }
  return nullptr;
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Get(
  I const *sym_name) const
  -> S* {
  // Get a symbol from the table, borrowed rather than owned,
  // so a lookup costs no refcount traffic.
  if (sym_name == nullptr) { return nullptr; }
  if (_Table.empty()) { return nullptr; }
  auto ptr = _Table.find(SymbolKey(sym_name));
  return ptr != _Table.end() ? ptr->second.get() : nullptr;
}
template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::All() const
  -> Vec<S*> {
  // Generate all symbols in the table.
  return _Table
    | genex::views::transform([](auto const &pair) { return pair.second.get(); })
    | genex::to<Vec>();
}

spp::analyse::scopes::SymbolTable::SymbolTable() = default;

spp::analyse::scopes::SymbolTable::~SymbolTable() = default;

auto spp::analyse::scopes::SymbolTable::ShallowCopyFrom(
  SymbolTable const &that)
  -> void {
  // Share the symbols of every table.
  NsTbl.ShallowCopyFrom(that.NsTbl);
  TypeTbl.ShallowCopyFrom(that.TypeTbl);
  VarTbl.ShallowCopyFrom(that.VarTbl);
}

auto spp::analyse::scopes::SymbolTable::DeepCopyFrom(
  SymbolTable const &that)
  -> void {
  // Copy the symbols of every table.
  NsTbl.DeepCopyFrom(that.NsTbl);
  TypeTbl.DeepCopyFrom(that.TypeTbl);
  VarTbl.DeepCopyFrom(that.VarTbl);
}

template class spp::analyse::scopes::IndividualSymbolTable<
  spp::asts::IdentifierAst, spp::analyse::scopes::NamespaceSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<
  spp::asts::TypeIdentifierAst, spp::analyse::scopes::TypeSymbol>;
template class spp::analyse::scopes::IndividualSymbolTable<
  spp::asts::IdentifierAst, spp::analyse::scopes::VariableSymbol>;
SPP_MOD_END
