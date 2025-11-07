#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/unary_expression_ast.hpp>
#include <spp/asts/unary_expression_operator_ast.hpp>


spp::asts::UnaryExpressionAst::UnaryExpressionAst(
    decltype(op) &&tok_op,
    decltype(expr) &&expr) :
    op(std::move(tok_op)),
    expr(std::move(expr)) {
}


spp::asts::UnaryExpressionAst::~UnaryExpressionAst() = default;


auto spp::asts::UnaryExpressionAst::pos_start() const -> std::size_t {
    return op->pos_start();
}


auto spp::asts::UnaryExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::UnaryExpressionAst::clone() const -> std::unique_ptr<Ast> {
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


auto spp::asts::UnaryExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::UnaryExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Analyse the semantics of the right-hand-side.
    ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Analyse the operator and right-hand-side expression.
    expr->stage_7_analyse_semantics(sm, meta);

    meta->save();
    meta->unary_expression_rhs = expr.get();
    op->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::UnaryExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Check the memory of the right-hand-side.
    expr->stage_8_check_memory(sm, meta);
}


auto spp::asts::UnaryExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> {
    // Infer the type of the right-hand-side expression, adjusted by the operator.
    meta->save();
    meta->unary_expression_rhs = expr.get();
    auto type = op->infer_type(sm, meta);
    meta->restore();

    return type;
}
