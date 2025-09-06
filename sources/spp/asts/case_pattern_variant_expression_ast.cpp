#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::CasePatternVariantExpressionAst::CasePatternVariantExpressionAst(
    decltype(expr) &&expr) :
    expr(std::move(expr)) {
}


spp::asts::CasePatternVariantExpressionAst::~CasePatternVariantExpressionAst() = default;


auto spp::asts::CasePatternVariantExpressionAst::pos_start() const -> std::size_t {
    return expr->pos_start();
}


auto spp::asts::CasePatternVariantExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::CasePatternVariantExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantExpressionAst>(ast_clone(expr));
}


spp::asts::CasePatternVariantExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward analysis into the expression.
    ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the expression.
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*expr, *expr, *sm, true, true, true, true, true, true, meta);
}
