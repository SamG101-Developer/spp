#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/gen_with_expression_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


spp::asts::GenWithExpressionAst::GenWithExpressionAst(
    decltype(tok_gen) &&tok_gen,
    decltype(tok_with) &&tok_with,
    decltype(expr) &&expr) :
    PrimaryExpressionAst(),
    tok_gen(std::move(tok_gen)),
    tok_with(std::move(tok_with)),
    expr(std::move(expr)) {
}


spp::asts::GenWithExpressionAst::~GenWithExpressionAst() = default;


auto spp::asts::GenWithExpressionAst::pos_start() const -> std::size_t {
    return tok_gen->pos_start();
}


auto spp::asts::GenWithExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::GenWithExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenWithExpressionAst>(
        ast_clone(tok_gen),
        ast_clone(tok_with),
        ast_clone(expr));
}


spp::asts::GenWithExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_gen);
    SPP_STRING_APPEND(tok_with);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::GenWithExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_gen);
    SPP_PRINT_APPEND(tok_with);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::GenWithExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the expression.
    ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Check the enclosing function is a coroutine and not a subroutine.
    const auto function_flavour = meta->enclosing_function_flavour;
    if (function_flavour->token_type != lex::SppTokenType::KW_COR) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionSubroutineContainsGenExpressionError>().with_args(
            *function_flavour, *tok_gen).with_scopes({sm->current_scope}).raise();
    }

    // Analyse the expression (guaranteed to exist), and determine the type of the expression.
    std::shared_ptr expr_type = generate::common_types::void_type(pos_start());
    meta->save();
    RETURN_TYPE_OVERLOAD_HELPER(expr.get()) {
        meta->return_type_overload_resolver_type = meta->enclosing_function_ret_type[0];
    }

    meta->assignment_target_type = meta->enclosing_function_ret_type.empty() ? nullptr : meta->enclosing_function_ret_type[0];
    meta->prevent_auto_generator_resume = true;
    expr->stage_7_analyse_semantics(sm, meta);
    expr_type = expr->infer_type(sm, meta);
    meta->restore();

    // Functions provide the return type, closures require inference; handle the inference.
    if (meta->enclosing_function_ret_type.empty()) {
        m_generator_type = generate::common_types::gen_type(expr->pos_start(), expr_type);
        m_generator_type->stage_7_analyse_semantics(sm, meta);
        meta->enclosing_function_ret_type.emplace_back(m_generator_type);
        meta->enclosing_function_scope = sm->current_scope;
    }
    else {
        m_generator_type = meta->enclosing_function_ret_type[0];
    }

    // Determine the "yield" type of the enclosing function (to type check the expression against).
    auto [_, yield_type, _, _, _, error_type] = analyse::utils::type_utils::get_generator_and_yield_type(
        *meta->enclosing_function_ret_type[0], *sm, *meta->enclosing_function_ret_type[0], "coroutine");

    // The expression type must be a Gen type that exactly matches the function_ret_type.
    if (not analyse::utils::type_utils::symbolic_eq(*meta->enclosing_function_ret_type[0], *expr_type, *meta->enclosing_function_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
            *meta->enclosing_function_ret_type[0], *meta->enclosing_function_ret_type[0], *expr, *expr_type).with_scopes({meta->enclosing_function_scope, sm->current_scope}).raise();
    }
}


auto spp::asts::GenWithExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the expression for memory issues.
    // Ensure the argument isn't moved or partially moved (for all conventions)
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*expr, *tok_gen, sm, true, true, false, false, false, false, meta);
}


auto spp::asts::GenWithExpressionAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Get the "Send" generic type parameter from the generator type.
    auto send_type = m_generator_type->type_parts().back()->generic_arg_group->type_at("Send")->val;
    return send_type;
}
