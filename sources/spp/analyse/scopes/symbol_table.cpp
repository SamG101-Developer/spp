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
  // Add a symbol to the table. Use string_view for the
  // find to avoid a copy.
  const auto sv = sym_name->ToView();
  auto it = _Table.find(sv);
  if (it != _Table.end()) {
    it->second = sym;
  }
  else {
    _Table.emplace(sv, sym);
  }
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Rem(
  I const *sym_name)
  -> Shared<S> {
  // Remove a symbol from the table.
  auto it = _Table.find(sym_name->ToView());
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
  -> Shared<S> {
  // Get a symbol from the table. Use string_view to avoid
  // a string copy per lookup.
  if (sym_name == nullptr) { return nullptr; }
  if (_Table.empty()) { return nullptr; }
  auto ptr = _Table.find(sym_name->ToView());
  return ptr != _Table.end() ? ptr->second : nullptr;
}

template <typename I, typename S>
auto spp::analyse::scopes::IndividualSymbolTable<I, S>::Has(
  I const *sym_name) const
  -> bool {
  // Check if a symbol exists in the table.
  if (sym_name == nullptr) { return false; }
  if (_Table.empty()) { return false; }
  auto ptr = _Table.find(sym_name->ToView());
  return ptr != _Table.end();
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
