module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.unary_expression_operator_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::UnaryExpressionAst::UnaryExpressionAst(
    decltype(op) &&tok_op,
    decltype(expr) &&expr) :
    op(std::move(tok_op)),
    expr(std::move(expr)) {
}


spp::asts::UnaryExpressionAst::~UnaryExpressionAst() = default;


auto spp::asts::UnaryExpressionAst::pos_start() const
    -> std::size_t {
    return op->pos_start();
}


auto spp::asts::UnaryExpressionAst::pos_end() const
    -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::UnaryExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<UnaryExpressionAst>(
        ast_clone(op),
        ast_clone(expr));
}


spp::asts::UnaryExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(op);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::UnaryExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the semantics of the right-hand-side.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Analyse the operator and right-hand-side expression.
    expr->stage_7_analyse_semantics(sm, meta);

    meta->save();
    meta->unary_expression_rhs = expr.get();
    op->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::UnaryExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the right-hand-side.
    expr->stage_8_check_memory(sm, meta);
}


auto spp::asts::UnaryExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type of the right-hand-side expression, adjusted by the operator.
    meta->save();
    meta->unary_expression_rhs = expr.get();
    auto type = op->infer_type(sm, meta);
    meta->restore();

    return type;
}
