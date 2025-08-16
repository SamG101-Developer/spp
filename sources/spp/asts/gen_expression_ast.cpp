#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/gen_expression_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


spp::asts::GenExpressionAst::GenExpressionAst(
    decltype(tok_gen) &&tok_gen,
    decltype(conv) &&conv,
    decltype(expr) &&expr) :
    PrimaryExpressionAst(),
    tok_gen(std::move(tok_gen)),
    conv(std::move(conv)),
    expr(std::move(expr)) {
}


spp::asts::GenExpressionAst::~GenExpressionAst() = default;


auto spp::asts::GenExpressionAst::pos_start() const -> std::size_t {
    return tok_gen->pos_start();
}


auto spp::asts::GenExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::GenExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenExpressionAst>(
        ast_clone(tok_gen),
        ast_clone(conv),
        ast_clone(expr));
}


spp::asts::GenExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_gen);
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::GenExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_gen);
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::GenExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the expression.
    ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Check the enclosing function is a coroutine and not a subroutine.
    const auto function_flavour = meta->enclosing_function_flavour;
    if (function_flavour->token_type != lex::SppTokenType::KW_COR) {
        analyse::errors::SppFunctionSubroutineContainsGenExpressionError(*function_flavour, *tok_gen)
            .scopes({sm->current_scope})
            .raise();
    }

    // Analyse the expression if it exists, and determine the type of the expression.
    std::shared_ptr expr_type = generate::common_types::void_type(pos_start());
    if (expr != nullptr) {
        meta->save();
        RETURN_TYPE_OVERLOAD_HELPER(expr.get()) {
            auto [gem_type, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
                *meta->enclosing_function_ret_type[0], *sm, *meta->enclosing_function_ret_type[0], "coroutine");
            meta->return_type_overload_resolver_type = std::move(yield_type);
        }

        meta->assignment_target_type = meta->enclosing_function_ret_type.empty() ? nullptr : meta->enclosing_function_ret_type[0];
        expr->stage_7_analyse_semantics(sm, meta);
        expr_type = expr->infer_type(sm, meta);
        meta->restore();
    }

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
    auto [_, yield_type, _, is_optional, is_fallible, error_type] = analyse::utils::type_utils::get_generator_and_yield_type(
        *meta->enclosing_function_ret_type[0], *sm, *meta->enclosing_function_ret_type[0], "coroutine");
    const auto direct_match = analyse::utils::type_utils::symbolic_eq(*yield_type, *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
    const auto optional_match = is_optional and analyse::utils::type_utils::symbolic_eq(*generate::common_types::void_type(0), *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
    const auto fallible_match = is_fallible and analyse::utils::type_utils::symbolic_eq(*error_type, *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
    if (not(direct_match or optional_match or fallible_match)) {
        analyse::errors::SppYieldedTypeMismatchError(*yield_type, *yield_type, *expr, *expr_type, is_optional, is_fallible, *error_type)
            .scopes({sm->current_scope})
            .raise();
    }
}


auto spp::asts::GenExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // If there is no expression, then now ork needs to be done.
    if (expr == nullptr) return;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*expr, *tok_gen, sm, true, true, false, false, false, false, meta);

    // If the value is non-symbolic, then there is no borrow logic to implement, so return.
    auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*expr);
    if (sym == nullptr) { return; }

    if (conv == nullptr) {
        // Ensure that attributes aren't being moved off of a borrowed value and that pins are maintained. Mark the move
        // or partial move of the argument.
        analyse::utils::mem_utils::validate_symbol_memory(*expr, *tok_gen, sm, false, false, true, true, false, true, meta);
    }

    else if (ast_cast<ConventionMutAst>(conv.get()) and not sym->is_mutable) {
        // Check the argument's symbol is mutable, if the symbol exists.
        analyse::errors::SppInvalidMutationError(*expr, *conv, *sym->memory_info->ast_initialization)
            .scopes({sm->current_scope})
            .raise();
    }
}


auto spp::asts::GenExpressionAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Get the "Send" generic type parameter from the generator type.
    auto send_type = m_generator_type->type_parts().back()->generic_args->type_at("Send")->val;
    return send_type;
}
