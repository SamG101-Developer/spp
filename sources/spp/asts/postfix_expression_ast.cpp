#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::PostfixExpressionAst::PostfixExpressionAst(
    decltype(lhs) &&lhs,
    decltype(op) &&op) :
    lhs(std::move(lhs)),
    op(std::move(op)) {
}


spp::asts::PostfixExpressionAst::~PostfixExpressionAst() = default;


auto spp::asts::PostfixExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::PostfixExpressionAst::pos_end() const -> std::size_t {
    return op->pos_end();
}


auto spp::asts::PostfixExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionAst>(
        ast_clone(lhs),
        ast_clone(op));
}


spp::asts::PostfixExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(op);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(op);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the lhs.
    ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TYPE(lhs.get());

    meta->save();
    meta->return_type_overload_resolver_type = nullptr;
    meta->prevent_auto_generator_resume = false;
    lhs->stage_7_analyse_semantics(sm, meta);

    // Re-attach the meta info, as it is targeting the lhs.
    meta->restore();
    op->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::PostfixExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the lhs.
    lhs->stage_8_check_memory(sm, meta);
    op->stage_8_check_memory(sm, meta);
}
