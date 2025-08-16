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
#include <genex/views/ptr.hpp>
#include <genex/views/remove.hpp>
#include <genex/views/to.hpp>
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
        ast_clone(tok_case),
        ast_clone(cond),
        ast_clone(tok_of),
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
    auto scope_name = analyse::scopes::ScopeBlockName("<case-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

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

    // Move into the "case" scope and check the memory satus of the symbols in the branches.
    sm->move_to_next_scope();
    analyse::utils::mem_utils::validate_inconsistent_memory(
        branches | genex::views::ptr_unique | genex::views::to<std::vector>(), sm, meta);

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Ensure consistency across branches.
    auto [master_branch_type_info, branches_type_info] = analyse::utils::type_utils::validate_inconsistent_types(
        branches, sm, meta);

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
