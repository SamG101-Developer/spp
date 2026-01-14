module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.pattern_guard_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::PatternGuardAst::PatternGuardAst(
    decltype(tok_and) &&tok_and,
    decltype(expr) &&expression) :
    tok_and(std::move(tok_and)),
    expr(std::move(expression)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_and, lex::SppTokenType::KW_AND, "and", expr ? expr->pos_start() : 0);
}


spp::asts::PatternGuardAst::~PatternGuardAst() = default;


auto spp::asts::PatternGuardAst::pos_start() const
    -> std::size_t {
    return tok_and->pos_start();
}


auto spp::asts::PatternGuardAst::pos_end() const
    -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::PatternGuardAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::PatternGuardAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the expression in the pattern guard.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);

    // Check the guard's type is boolean.
    const auto expr_type = expr->infer_type(sm, meta);
    if (not analyse::utils::type_utils::is_type_boolean(*expr_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotBooleanError>()
            .with_args(*expr, *expr_type, "pattern guard")
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::PatternGuardAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the expression.
    // Todo: how is this even applied? just truth check => barely any mem checks needed
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *this, *sm, true, true, false, false, false, false, meta);
}


auto spp::asts::PatternGuardAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the expression.
    return expr->stage_10_code_gen_2(sm, meta, ctx);
}
