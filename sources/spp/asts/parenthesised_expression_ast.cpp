module;
#include <spp/macros.hpp>

module spp.asts.parenthesised_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;


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


auto spp::asts::ParenthesisedExpressionAst::pos_start() const
    -> std::size_t {
    return tok_open_paren->pos_start();
}


auto spp::asts::ParenthesisedExpressionAst::pos_end() const
    -> std::size_t {
    return tok_close_paren->pos_end();
}


auto spp::asts::ParenthesisedExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::ParenthesisedExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_open_paren);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_APPEND(tok_close_paren);
    SPP_PRINT_END;
}


auto spp::asts::ParenthesisedExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ParenthesisedExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the expression.
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *this, *sm, true, true, true, false, false, false, meta);
}


auto spp::asts::ParenthesisedExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the inner expression.
    return expr->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::ParenthesisedExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the inner expression's type.
    return expr->infer_type(sm, meta);
}
