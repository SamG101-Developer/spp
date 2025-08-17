#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/case_pattern_variant_else_case_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
    decltype(tok_else) &&tok_else,
    decltype(case_expr) &&case_expr) :
    tok_else(std::move(tok_else)),
    case_expr(std::move(case_expr)) {
}


auto spp::asts::CasePatternVariantElseCaseAst::pos_start() const -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::CasePatternVariantElseCaseAst::pos_end() const -> std::size_t {
    return case_expr->pos_end();
}


auto spp::asts::CasePatternVariantElseCaseAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantElseCaseAst>(
        ast_clone(tok_else),
        ast_clone(case_expr));
}


spp::asts::CasePatternVariantElseCaseAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_APPEND(case_expr);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantElseCaseAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_APPEND(case_expr);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantElseCaseAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward analysis into the case expression.
    case_expr->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantElseCaseAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the case expression.
    case_expr->stage_8_check_memory(sm, meta);
}
