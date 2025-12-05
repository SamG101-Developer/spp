module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.gen_with_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.gen_expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.loop_condition_iterable_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.loop_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::GenWithExpressionAst::GenWithExpressionAst(
    decltype(tok_gen) &&tok_gen,
    decltype(tok_with) &&tok_with,
    decltype(expr) &&expr) :
    PrimaryExpressionAst(),
    tok_gen(std::move(tok_gen)),
    tok_with(std::move(tok_with)),
    expr(std::move(expr)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_gen, lex::SppTokenType::KW_GEN, "gen");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_with, lex::SppTokenType::KW_WITH, "with");
}


spp::asts::GenWithExpressionAst::~GenWithExpressionAst() = default;


auto spp::asts::GenWithExpressionAst::pos_start() const
    -> std::size_t {
    return tok_gen->pos_start();
}


auto spp::asts::GenWithExpressionAst::pos_end() const
    -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::GenWithExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenWithExpressionAst>(
        ast_clone(tok_gen),
        ast_clone(tok_with),
        ast_clone(expr));
}


spp::asts::GenWithExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_gen).append(" ");
    SPP_STRING_APPEND(tok_with).append(" ");
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::GenWithExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_gen).append(" ");
    SPP_PRINT_APPEND(tok_with).append(" ");
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::GenWithExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Check the enclosing function is a coroutine and not a subroutine.
    const auto function_flavour = meta->enclosing_function_flavour;
    if (function_flavour->token_type != lex::SppTokenType::KW_COR) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionSubroutineContainsGenExpressionError>()
            .with_args(*function_flavour, *tok_gen)
            .raises_from(sm->current_scope);
    }

    // Analyse the expression (guaranteed to exist), and determine the type of the expression.
    auto expr_type = generate::common_types::void_type(pos_start());
    meta->save();
    SPP_RETURN_TYPE_OVERLOAD_HELPER(expr.get()) {
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
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
            .with_args(*meta->enclosing_function_ret_type[0], *meta->enclosing_function_ret_type[0], *expr, *expr_type)
            .raises_from(meta->enclosing_function_scope, sm->current_scope);
    }
}


auto spp::asts::GenWithExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the expression for memory issues.
    // Ensure the argument isn't moved or partially moved (for all conventions)
    // Todo: Investigate pin checks here.
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *tok_gen, *sm, true, true, false, false, false, false, meta);
}


auto spp::asts::GenWithExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Build the iterable for the loop; the iterable is just the expression, mapped into a different AST.
    auto temp_var_name = std::make_unique<IdentifierAst>(0, "_coro_temp");
    auto temp_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(temp_var_name), nullptr);
    auto loop_cond_iter = std::make_unique<LoopConditionIterableAst>(std::move(temp_var), nullptr, std::move(expr));

    // Build the "gen" expression for the individual yielding.
    auto gen_value = std::make_unique<IdentifierAst>(0, "_coro_temp");
    auto gen_expression = std::make_unique<GenExpressionAst>(nullptr, nullptr, std::move(gen_value));
    auto loop_body = std::make_unique<decltype(LoopExpressionAst::body)::element_type>(nullptr, std::vector<std::unique_ptr<StatementAst>>(), nullptr);
    loop_body->members.emplace_back(std::unique_ptr<StatementAst>(gen_expression.release()));

    // Build the loop expression with the iterable condition.
    const auto loop_expr = std::make_unique<LoopExpressionAst>(nullptr, std::move(loop_cond_iter), std::move(loop_body), nullptr);
    return loop_expr->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::GenWithExpressionAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Get the "Send" generic type parameter from the generator type.
    auto send_type = m_generator_type->type_parts().back()->generic_arg_group->type_at("Send")->val;
    return send_type;
}
