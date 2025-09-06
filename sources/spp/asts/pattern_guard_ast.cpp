#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::PatternGuardAst::PatternGuardAst(
    decltype(tok_and) &&tok_and,
    decltype(expr) &&expression) :
    tok_and(std::move(tok_and)),
    expr(std::move(expression)) {
}


spp::asts::PatternGuardAst::~PatternGuardAst() = default;


auto spp::asts::PatternGuardAst::pos_start() const -> std::size_t {
    return tok_and->pos_start();
}


auto spp::asts::PatternGuardAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::PatternGuardAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<PatternGuardAst>(
        ast_clone(tok_and),
        ast_clone(expr));
}


spp::asts::PatternGuardAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_and);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::PatternGuardAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_and);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::PatternGuardAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the expression in the pattern guard.
    ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);

    // Check the guard's type is boolean.
    const auto expr_type = expr->infer_type(sm, meta);
    if (not analyse::utils::type_utils::is_type_boolean(*expr_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotBooleanError>().with_args(
            *expr, *expr_type, "pattern guard").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::PatternGuardAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the expression.
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*expr, *this, *sm, true, true, true, false, false, true, meta);
}
