#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/binary_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/for_each.hpp>


spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
    decltype(op) &&op,
    decltype(patterns) &&patterns,
    decltype(guard) &&guard,
    decltype(body) &&body) :
    op(std::move(op)),
    patterns(std::move(patterns)),
    guard(std::move(guard)),
    body(std::move(body)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->body);
}


spp::asts::CaseExpressionBranchAst::~CaseExpressionBranchAst() = default;


auto spp::asts::CaseExpressionBranchAst::pos_start() const -> std::size_t {
    return op ? op->pos_start() : patterns.front()->pos_start();
}


auto spp::asts::CaseExpressionBranchAst::pos_end() const -> std::size_t {
    return body->pos_end();
}


auto spp::asts::CaseExpressionBranchAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CaseExpressionBranchAst>(
        ast_clone(op),
        ast_clone_vec(patterns),
        ast_clone(guard),
        ast_clone(body));
}


spp::asts::CaseExpressionBranchAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(op);
    SPP_STRING_EXTEND(patterns);
    SPP_STRING_APPEND(guard);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionBranchAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_EXTEND(patterns);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}


auto spp::asts::CaseExpressionBranchAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    auto scope_name = analyse::scopes::ScopeBlockName("<case-pattern#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Analyse the patterns, ensuring comparison methods exist is needed.
    patterns | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });

    if (op.get() and op->token_type != lex::SppTokenType::KW_IS) {
        for (auto &&p : patterns) {
            // Create a dummy function to check the comparison operator exists.
            const auto pe = ast_cast<CasePatternVariantExpressionAst>(p.get());
            const auto bin_ast = std::make_unique<BinaryExpressionAst>(
                std::make_unique<ObjectInitializerAst>(meta->case_condition->infer_type(sm, meta), nullptr),
                ast_clone(op),
                std::make_unique<ObjectInitializerAst>(pe->expr->infer_type(sm, meta), nullptr));
            bin_ast->stage_7_analyse_semantics(sm, meta);
        }
    }

    // Analyse the guard and body.
    if (guard) {
        guard->stage_7_analyse_semantics(sm, meta);
    }
    body->stage_7_analyse_semantics(sm, meta);

    // Exit the scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionBranchAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Move into the branch's scope.
    sm->move_to_next_scope();

    // Check the patterns, guard and body.
    patterns | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
    if (guard) {
        guard->stage_8_check_memory(sm, meta);
    }
    body->stage_8_check_memory(sm, meta);

    // Move out of the branch's scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionBranchAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Forward type inference to the body.
    return body->infer_type(sm, meta);
}
