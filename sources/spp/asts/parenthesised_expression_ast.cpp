#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/parenthesised_expression.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::ParenthesisedExpressionAst::ParenthesisedExpressionAst(
    decltype(tok_open_paren) &&tok_open_paren,
    decltype(expr) &&expr,
    decltype(tok_close_paren) &&tok_close_paren) :
    PrimaryExpressionAst(),
    tok_open_paren(std::move(tok_open_paren)),
    expr(std::move(expr)),
    tok_close_paren(std::move(tok_close_paren)) {
}


spp::asts::ParenthesisedExpressionAst::~ParenthesisedExpressionAst() = default;


auto spp::asts::ParenthesisedExpressionAst::pos_start() const -> std::size_t {
    return tok_open_paren->pos_start();
}


auto spp::asts::ParenthesisedExpressionAst::pos_end() const -> std::size_t {
    return tok_close_paren->pos_end();
}


auto spp::asts::ParenthesisedExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ParenthesisedExpressionAst>(
        ast_clone(tok_open_paren),
        ast_clone(expr),
        ast_clone(tok_close_paren));
}


spp::asts::ParenthesisedExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_open_paren);
    SPP_STRING_APPEND(expr);
    SPP_STRING_APPEND(tok_close_paren);
    SPP_STRING_END;
}


auto spp::asts::ParenthesisedExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_open_paren);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_APPEND(tok_close_paren);
    SPP_PRINT_END;
}


auto spp::asts::ParenthesisedExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Forward analysis into the expression.
    ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ParenthesisedExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Check the memory of the expression.
    expr->stage_8_check_memory(sm, meta);
    // analyse::utils::mem_utils::validate_symbol_memory(
    //     *expr, *this, *sm, true, true, true, false, false, false, meta);
}


auto spp::asts::ParenthesisedExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> {
    // The type of a parenthesised expression is the type of the inner expression.
    return expr->infer_type(sm, meta);
}
