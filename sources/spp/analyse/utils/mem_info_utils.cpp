module;
#include <spp/macros.hpp>

module spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
auto MemoryInfo::InitializedBy(
  Ast const &ast,
  Scope *scope)
  -> void {
  AstInitialization = {&ast, scope};
  AstInitializationOrigin = {&ast, scope};
  AstMoved = {nullptr, nullptr};
  InitializationCounter += 1;

  IsInconsistentlyInitialized = std::nullopt;
  IsInconsistentlyMoved = std::nullopt;
  IsInconsistentlyPartiallyMoved = std::nullopt;
  IsInconsistentlyBorrowEscaping = std::nullopt;
}

auto MemoryInfo::MovedBy(
  Ast const &ast, Scope *scope)
  -> void {
  AstMoved = {&ast, scope};
  AstInitialization = {nullptr, nullptr};
}

auto MemoryInfo::RemovePartialMoves(
  Ast const &ast,
  Scope * /*scope*/)
  -> void {
  // Use "string" comparison; same as overlap checking mechanism.
  // Writing a moved-out part back puts only that part back: the
  // symbol is not re-initialised, which would clear what the
  // branches disagreed about while other parts are still missing.
  // Once no part is missing, none can be inconsistently missing.
  genex::actions::remove(
    AstPartialMoves, ast.ToString(),
    [](auto const &x) { return x->ToString(); });
  if (AstPartialMoves.IsEmpty()) {
    IsInconsistentlyPartiallyMoved = std::nullopt;
  }
}

auto MemoryInfo::Snapshot() const
  -> MemoryInfoSnapshot {
  // A snapshot is the saved part of this struct, so taking
  // one is just a copy of that part.
  return *this;
}

auto MemoryInfo::Clone() const
  -> Unique<MemoryInfo> {
  auto out = MakeUnique<MemoryInfo>();
  static_cast<MemoryState&>(*out) = *this;
  static_cast<MemoryConsistency&>(*out) = *this;
  out->AstInitializationOrigin = AstInitializationOrigin;
  out->AstBorrowed = AstBorrowed;
  return out;
}

auto MemoryInfo::FillFromSnapshot(
  MemoryInfoSnapshot const &snapshot)
  -> void {
  // Everything a snapshot holds is the saved part of this
  // struct, and nothing outside it is touched.
  static_cast<MemoryState&>(*this) = snapshot;
}

SPP_MOD_END
