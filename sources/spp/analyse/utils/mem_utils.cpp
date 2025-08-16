#include <map>

#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/mem_utils.hpp>

#include <genex/views/map.hpp>
#include <genex/views/to.hpp>


template <typename T>
auto spp::analyse::utils::mem_utils::validate_inconsistent_memory(
    std::vector<T> const &branches,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> void {
    // Define a simple alias for a list of symbols and their memory.
    using SymbolMemoryList = std::vector<std::pair<typename T::value_type::pointer, MemoryInfoSnapshot>>;

    auto sym_mem_info = std::map<scopes::VariableSymbol*, SymbolMemoryList>();
    for (auto &&branch : branches) {
        // Make a record of the symbols' memory status in the scope before the branch is analysed.
        auto var_symbols_in_scope = sm->current_scope->all_var_symbols();
        auto old_symbol_mem_info = var_symbols_in_scope
            | genex::views::map([sm](auto &&x) { return std::make_pair(x, sm->current_scope->get_var_symbol(*x->name)->memory_info->snapshot()); })
            | genex::views::to<std::vector>();

        // Analyse the memory and then recheck the symbols' memory status.
        branch->stage_8_check_memory(sm, meta);
        auto new_symbol_mem_info = var_symbols_in_scope
            | genex::views::map([sm](auto &&x) { return std::make_pair(x, sm->current_scope->get_var_symbol(*x->name)->memory_info->snapshot()); })
            | genex::views::to<std::vector>();

        // Reset the memory status of the symbols for the next branch to analyse with the same original memory states.
        for (auto &&[sym, old_mem_status] : old_symbol_mem_info) {
            sym->memory_info->ast_initialization = old_mem_status.ast_initialization;
            sym->memory_info->ast_moved = old_mem_status.ast_moved;
            sym->memory_info->ast_partial_moves = old_mem_status.ast_partial_moves;
            sym->memory_info->ast_pins = old_mem_status.ast_pins;
            sym->memory_info->initialization_counter = old_mem_status.initialization_counter;

            // Save this memory status for subsequent inter-branch status comparisons.
            sym_mem_info.try_emplace(sym, SymbolMemoryList()).first->second.emplace_back(branch.get(), old_mem_status);
        }
    }

    // Check for consistency among the branches' symbols' memory states.
    for (auto &&[sym, branches_memory_info_lists] : sym_mem_info) {
        auto first_branch = branches.front().get();
        auto first_branch_mem_info = branches_memory_info_lists.at(0).second;

        // Assuming all new memory states are consistent across branches, update o the first "new" state list.
        sym->memory_info->ast_initialization = first_branch_mem_info.ast_initialization;
        sym->memory_info->ast_moved = first_branch_mem_info.ast_moved;
        sym->memory_info->ast_partial_moves = first_branch_mem_info.ast_partial_moves;
        sym->memory_info->ast_pins = first_branch_mem_info.ast_pins;
        sym->memory_info->initialization_counter = first_branch_mem_info.initialization_counter;

        // Check the new memory status for each symbol is consistent across all branches.
        for (auto &&[branch, branch_memory_info_list] : branches_memory_info_lists) {
            // Check for consistent initialization.
            if ((first_branch_mem_info.ast_initialization == nullptr) != (branch_memory_info_list.ast_initialization == nullptr)) {
                sym->memory_info->is_inconsistently_initialized = std::make_pair(first_branch, branch);
            }

            // Check for consistent moved state.
            if ((first_branch_mem_info.ast_moved == nullptr) != (branch_memory_info_list.ast_moved == nullptr)) {
                sym->memory_info->is_inconsistently_moved = std::make_pair(first_branch, branch);
            }

            // Check for consistent partial moves.
            if (first_branch_mem_info.ast_partial_moves != branch_memory_info_list.ast_partial_moves) {
                sym->memory_info->is_inconsistently_partially_moved = std::make_pair(first_branch, branch);
            }

            // Check for consistent pins.
            if (first_branch_mem_info.ast_pins != branch_memory_info_list.ast_pins) {
                sym->memory_info->is_inconsistently_pinned = std::make_pair(first_branch, branch);
            }
        }
    }
}
