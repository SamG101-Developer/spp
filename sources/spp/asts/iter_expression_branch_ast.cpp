module;
#include <spp/macros.hpp>

module spp.asts.iter_expression_branch_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.inner_scope_expression_ast;
import spp.asts.iter_pattern_variant_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.pattern_guard_ast;


spp::asts::IterExpressionBranchAst::IterExpressionBranchAst(
    decltype(pattern) &&pattern,
    decltype(body) &&body,
    decltype(guard) &&guard) :
    pattern(std::move(pattern)),
    body(std::move(body)),
    guard(std::move(guard)) {
}


spp::asts::IterExpressionBranchAst::~IterExpressionBranchAst() = default;


auto spp::asts::IterExpressionBranchAst::pos_start() const
    -> std::size_t {
    return pattern->pos_start();
}


auto spp::asts::IterExpressionBranchAst::pos_end() const
    -> std::size_t {
    return body->pos_end();
}


auto spp::asts::IterExpressionBranchAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterExpressionBranchAst>(
        ast_clone(pattern),
        ast_clone(body),
        ast_clone(guard));
}


spp::asts::IterExpressionBranchAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(pattern);
    SPP_STRING_APPEND(body);
    SPP_STRING_APPEND(guard);
    SPP_STRING_END;
}


auto spp::asts::IterExpressionBranchAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(pattern);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_END;
}


auto spp::asts::IterExpressionBranchAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the scope for the iteration branch.
    auto scope_name = analyse::scopes::ScopeBlockName("<iter-branch#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Analyse the pattern, guard and body.
    pattern->stage_7_analyse_semantics(sm, meta);
    if (guard) { guard->stage_7_analyse_semantics(sm, meta); }
    body->stage_7_analyse_semantics(sm, meta);

    // Exit the scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionBranchAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the branch's scope.
    sm->move_to_next_scope();

    // Check the patterns, guard and body.
    pattern->stage_8_check_memory(sm, meta);
    if (guard) { guard->stage_8_check_memory(sm, meta); }
    body->stage_8_check_memory(sm, meta);

    // Move out of the branch's scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionBranchAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // The type of the branch is the type of the body.
    return body->infer_type(sm, meta);
}
