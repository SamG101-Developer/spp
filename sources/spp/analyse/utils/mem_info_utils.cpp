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
    IsInconsistentlyPinned = std::nullopt;
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
    // Create and return the snapshot.
    return MemoryInfoSnapshot(
        std::get<0>(AstInitialization), std::get<1>(AstInitialization),
        std::get<0>(AstMoved), std::get<1>(AstMoved),
        AstPartialMoves, AstPins, AstEscapingBorrows, InitializationCounter);
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::Clone() const
    -> Unique<MemoryInfo> {
    auto out = MakeUnique<MemoryInfo>();
    out->AstInitialization = AstInitialization;
    out->AstMoved = AstMoved;
    out->AstInitializationOrigin = AstInitializationOrigin;
    out->AstBorrowed = AstBorrowed;
    out->AstPartialMoves = AstPartialMoves;
    out->AstPins = AstPins;
    out->AstEscapingBorrows = AstEscapingBorrows;
    out->AstCompTime = asts::AstClone(AstCompTime);
    out->InitializationCounter = InitializationCounter;
    out->IsInconsistentlyInitialized = IsInconsistentlyInitialized;
    out->IsInconsistentlyMoved = IsInconsistentlyMoved;
    out->IsInconsistentlyPartiallyMoved = IsInconsistentlyPartiallyMoved;
    out->IsInconsistentlyPinned = IsInconsistentlyPinned;
    return out;
}

auto spp::analyse::utils::mem_info_utils::MemoryInfo::FillFromSnapshot(
    MemoryInfoSnapshot const &snapshot)
    -> void {
    AstInitialization = {snapshot.AstInitialization, snapshot.ScopeInitialization};
    AstMoved = {snapshot.AstMoved, snapshot.ScopeMoved};
    AstPartialMoves = snapshot.AstPartialMoves;
    AstPins = snapshot.AstPins;
    AstEscapingBorrows = snapshot.AstEscapingBorrows;
    InitializationCounter = snapshot.InitializationCounter;
}

SPP_MOD_END
