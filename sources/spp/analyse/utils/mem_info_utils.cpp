module;
#include <spp/macros.hpp>

module spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
auto spp::analyse::utils::mem_info_utils::MemoryInfo::InitializedBy(
  asts::Ast const &ast,
  scopes::Scope *scope)
  -> void {
  AstInitialization = {&ast, scope};
  AstInitializationOrigin = {&ast, scope};
  AstMoved = {nullptr, nullptr};
  InitializationCounter += 1;

  IsInconsistentlyInitialized = std::nullopt;
  IsInconsistentlyMoved = std::nullopt;
  IsInconsistentlyPartiallyMoved = std::nullopt;
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::MovedBy(
  asts::Ast const &ast, scopes::Scope *scope)
  -> void {
  AstMoved = {&ast, scope};
  AstInitialization = {nullptr, nullptr};
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::RemovePartialMoves(
  asts::Ast const &ast,
  scopes::Scope *scope)
  -> void {
  // Use "string" comparison; same as overlap checking mechanism.
  genex::actions::remove(
    AstPartialMoves, ast.ToString(),
    [](auto const &x) { return x->ToString(); });
  if (not AstPartialMoves.IsEmpty()) {
    InitializedBy(ast, scope);
  }
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::Snapshot() const
  -> MemoryInfoSnapshot {
  // A snapshot is the saved part of this struct, so taking
  // one is just a copy of that part.
  return *this;
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::Clone() const
  -> Unique<MemoryInfo> {
  auto out = MakeUnique<MemoryInfo>();
  static_cast<MemoryState&>(*out) = *this;
  static_cast<MemoryConsistency&>(*out) = *this;
  out->AstInitializationOrigin = AstInitializationOrigin;
  out->AstBorrowed = AstBorrowed;
  out->AstCompTime = asts::AstClone(AstCompTime);
  return out;
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::FillFromSnapshot(
  MemoryInfoSnapshot const &snapshot)
  -> void {
  // Everything a snapshot holds is the saved part of this
  // struct, and nothing outside it is touched.
  static_cast<MemoryState&>(*this) = snapshot;
}

SPP_MOD_END
