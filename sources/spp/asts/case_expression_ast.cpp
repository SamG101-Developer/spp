#include <algorithm>
#include <map>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/boolean_literal_ast.hpp>
#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/case_pattern_variant_else_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>

#include <genex/actions/remove.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/remove.hpp>
#include <genex/algorithms/any_of.hpp>
#include <genex/algorithms/find.hpp>


spp::asts::CaseExpressionAst::CaseExpressionAst(
    decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches) :
    tok_case(std::move(tok_case)),
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
}


auto spp::asts::CaseExpressionAst::new_non_pattern_match(
    decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> &&first,
    decltype(branches) &&branches) -> std::unique_ptr<CaseExpressionAst> {
    // Convert consecutive if/else-if/else branches into case pattern matching.
    auto patterns = std::vector<std::unique_ptr<CasePatternVariantAst>>(1);
    patterns.push_back(std::make_unique<CasePatternVariantExpressionAst>(BooleanLiteralAst::True(tok_case->pos_start())));
    auto first_branch = std::make_unique<CaseExpressionBranchAst>(nullptr, std::move(patterns), nullptr, std::move(first));
    branches.insert(branches.begin(), std::move(first_branch));

    // Return the final, newly created, case expression AST.
    auto out = std::make_unique<CaseExpressionAst>(std::move(tok_case), std::move(cond), nullptr, std::move(branches));
    return out;
}


auto spp::asts::CaseExpressionAst::pos_start() const -> std::size_t {
    return tok_case->pos_start();
}


auto spp::asts::CaseExpressionAst::pos_end() const -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::CaseExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CaseExpressionAst>(
        ast_clone(*tok_case),
        ast_clone(*cond),
        ast_clone(*tok_of),
        ast_clone_vec(branches));
}


spp::asts::CaseExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_case);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(tok_of);
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_case);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(tok_of);
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}


auto spp::asts::CaseExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the condition expression.
    ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Create the scope for the case expression.
    sm->create_and_move_into_new_scope(analyse::scopes::ScopeBlockName("<case-expr#" + std::to_string(pos_start()) + ">"), this);

    // Analyse eac branch of the case expression.
    for (auto &&branch : branches) {
        // Destructures can only use 1 pattern.
        if (branch->op != nullptr and branch->op->token_type == lex::SppTokenType::KW_IS and branch->patterns.size() > 1) {
            analyse::errors::SppCaseBranchMultipleDestructuresError(*branch->patterns[0], *branch->patterns[1]).scopes({sm->current_scope}).raise();
        }

        // Check the "else" branch is the last branch (also checks there is only 1 "else" branch).
        if (ast_cast<CasePatternVariantAst>(branch->patterns[0].get()) and branch != branches.back()) {
            analyse::errors::SppCaseBranchElseNotLastError(*branch, *branches.back()).scopes({sm->current_scope}).raise();
        }

        // Analyse the branch.
        meta->save();
        meta->case_condition = cond.get();
        branch->stage_7_analyse_semantics(sm, meta);
        meta->restore();
    }

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Check the memory state of the condition.
    cond->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*cond, *cond, sm, true, true, true, false, false, false, meta);
    using SymbolMemoryList = std::vector<std::pair<CaseExpressionBranchAst*, analyse::utils::mem_utils::MemoryInfoSnapshot>>;

    // Move into the "case" scope and check the memory satus of the symbols in the branches.
    sm->move_out_of_current_scope();
    auto sym_mem_info = std::map<analyse::scopes::VariableSymbol*, SymbolMemoryList>();
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

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::map([sm, meta](auto &&x) { return std::make_pair(x.get(), x->infer_type(sm, meta)); })
        | genex::views::to<std::vector>();

    // Filter the branch types down to variant types for custom analysis.
    auto variant_branches_type_info = branches_type_info
        | genex::views::filter([sm](auto &&x) { return analyse::utils::type_utils::is_type_variant(*x.second, *sm->current_scope); })
        | genex::views::to<std::vector>();

    // Set the master branch type to the first branch's type, if it exists. This is the default and may be subsequently changed.
    auto master_branch_type_info = not branches.empty()
                                       ? std::make_pair(branches_type_info[0].first, branches_type_info[0].second.get())
                                       : std::make_pair(nullptr, nullptr);

    // Override the master type if a pre-provided type (for assignment) has been given.
    if (meta->assignment_target_type != nullptr) {
        master_branch_type_info = std::make_pair(nullptr, meta->assignment_target_type.get());
    }

    // Otherwise, if there are variant branches, use the most variant type as the master branch type.
    else if (not variant_branches_type_info.empty()) {
        auto most_inner_types = 0uz;
        for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
            const auto variant_size = variant_type->type_parts().back()->generic_args->type_at("Variant")->val->type_parts().back()->generic_args->args.size();
            if (variant_size > most_inner_types) {
                master_branch_type_info = std::make_tuple(variant_branch, variant_type.get());
                most_inner_types = variant_size;
            }
        }
    }

    // Remove the master branch pointer from the list of remaining branch types and check all types match.
    auto mismatch_branches_type_info = branches_type_info
        | genex::views::remove_if([master_branch_type_info](auto &&x) { return x.first == master_branch_type_info.first; })
        | genex::views::filter([master_branch_type_info, sm](auto &&x) { return not analyse::utils::type_utils::symbolic_eq(*master_branch_type_info.second, *x.second, *sm->current_scope, *sm->current_scope); })
        | genex::views::to<std::vector>();

    if (mismatch_branches_type_info.empty()) {
        auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
        auto [master_branch, master_branch_type] = master_branch_type_info;
        analyse::errors::SppTypeMismatchError(*master_branch->body->final_member(), *master_branch_type, *mismatch_branch->body->final_member(), *mismatch_branch_type)
            .scopes({sm->current_scope})
            .raise();
    }

    // Ensure there is an "else" branch if the branches are not exhaustive. Todo: Need to investigate how to detect exhaustion.
    if (ast_cast<CasePatternVariantElseAst>(branches.back()->patterns[0].get()) == nullptr and not meta->ignore_missing_else_branch_for_inference) {
        analyse::errors::SppCaseBranchMissingElseError(*this, *branches.back())
            .scopes({sm->current_scope})
            .raise();
    }

    // Return the branches' return type. If there are any branches, otherwise Void.
    if (branches_type_info.empty()) {
        return generate::common_types::void_type(pos_start());
    }

    auto final_type_info = branches_type_info | genex::views::filter([master_branch_type_info](auto &&x) { return x.second.get() == master_branch_type_info.second; });
    for (auto &&[_, final_type] : final_type_info) {
        return std::move(final_type);
    }
    return generate::common_types::void_type(pos_start());
}
