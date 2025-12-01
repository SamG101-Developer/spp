module spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.utils.ast_utils;
import genex;


auto spp::analyse::utils::mem_info_utils::MemoryInfo::initialized_by(
    asts::Ast const &ast,
    scopes::Scope *scope)
    -> void {
    ast_initialization = {&ast, scope};
    ast_initialization_origin = {&ast, scope};
    ast_moved = {nullptr, nullptr};
    initialization_counter += 1;

    is_inconsistently_initialized = std::nullopt;
    is_inconsistently_moved = std::nullopt;
    is_inconsistently_partially_moved = std::nullopt;
    is_inconsistently_pinned = std::nullopt;
}


auto spp::analyse::utils::mem_info_utils::MemoryInfo::moved_by(
    asts::Ast const &ast, scopes::Scope *scope)

    -> void {
    ast_moved = {&ast, scope};
    ast_initialization = {nullptr, nullptr};
}


auto spp::analyse::utils::mem_info_utils::MemoryInfo::remove_partial_move(
    asts::Ast const &ast,
    scopes::Scope *scope)
    -> void {
    genex::actions::remove(ast_partial_moves, &ast);
    if (not ast_partial_moves.empty()) {
        initialized_by(ast, scope);
    }
}


auto spp::analyse::utils::mem_info_utils::MemoryInfo::snapshot() const
    -> MemoryInfoSnapshot {
    // Create and return the snapshot.
    return MemoryInfoSnapshot(
        std::get<0>(ast_initialization), std::get<0>(ast_moved), ast_partial_moves, ast_pins, ast_linked_pins,
        initialization_counter);
}


auto spp::analyse::utils::mem_info_utils::MemoryInfo::clone() const
    -> std::unique_ptr<MemoryInfo> {
    auto out = std::make_unique<MemoryInfo>();
    out->ast_initialization = ast_initialization;
    out->ast_moved = ast_moved;
    out->ast_initialization_origin = ast_initialization_origin;
    out->ast_borrowed = ast_borrowed;
    out->ast_partial_moves = ast_partial_moves;
    out->ast_pins = ast_pins;
    out->ast_comptime = asts::ast_clone(ast_comptime);
    out->initialization_counter = initialization_counter;
    out->is_inconsistently_initialized = is_inconsistently_initialized;
    out->is_inconsistently_moved = is_inconsistently_moved;
    out->is_inconsistently_partially_moved = is_inconsistently_partially_moved;
    out->is_inconsistently_pinned = is_inconsistently_pinned;
    return out;
}
