module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.gen_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.lex.tokens;

import llvm;


spp::asts::GenExpressionAst::GenExpressionAst(
    decltype(tok_gen) &&tok_gen,
    decltype(conv) &&conv,
    decltype(expr) &&expr) :
    PrimaryExpressionAst(),
    tok_gen(std::move(tok_gen)),
    conv(std::move(conv)),
    expr(std::move(expr)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_gen, lex::SppTokenType::KW_GEN, "gen");
}


spp::asts::GenExpressionAst::~GenExpressionAst() = default;


auto spp::asts::GenExpressionAst::pos_start() const
    -> std::size_t {
    return tok_gen->pos_start();
}


auto spp::asts::GenExpressionAst::pos_end() const
    -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::GenExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::GenExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_gen);
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::GenExpressionAst::stage_7_analyse_semantics(
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

    // Analyse the expression if it exists, and determine the type of the expression.
    auto expr_type = generate::common_types::void_type(pos_start());
    if (expr != nullptr) {
        meta->save();
        SPP_RETURN_TYPE_OVERLOAD_HELPER(expr.get()) {
            auto [gen_type, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
                *meta->enclosing_function_ret_type[0], *sm->current_scope, *meta->enclosing_function_ret_type[0], "coroutine");
            meta->return_type_overload_resolver_type = std::move(yield_type);
        }

        meta->assignment_target_type = meta->enclosing_function_ret_type.empty() ? nullptr : meta->enclosing_function_ret_type[0];
        expr->stage_7_analyse_semantics(sm, meta);
        expr_type = expr->infer_type(sm, meta)->with_convention(ast_clone(conv));
        meta->restore();
    }

    // Functions provide the return type, closures require inference; handle the inference.
    if (meta->enclosing_function_ret_type.empty()) {
        m_gen_type = generate::common_types::gen_type(expr->pos_start(), expr_type);
        m_gen_type->stage_7_analyse_semantics(sm, meta);
        meta->enclosing_function_ret_type.emplace_back(m_gen_type);
    }
    else {
        m_gen_type = meta->enclosing_function_ret_type[0];
    }

    // Determine the "Yield" type of the enclosing function (to type check the expression against).
    auto [_, yield_type, _, is_optional, is_fallible, error_type] = analyse::utils::type_utils::get_generator_and_yield_type(
        *m_gen_type, *sm->current_scope, *m_gen_type, "coroutine");
    const auto direct_match = analyse::utils::type_utils::symbolic_eq(*yield_type, *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
    const auto optional_match = is_optional and analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::VOID, *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
    const auto fallible_match = is_fallible and analyse::utils::type_utils::symbolic_eq(*error_type, *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
    if (not(direct_match or optional_match or fallible_match)) {
        error_type = error_type ? error_type : generate::common_types_precompiled::VOID;
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppYieldedTypeMismatchError>()
            .with_args(*yield_type, *yield_type, expr ? *expr->to<Ast>() : *tok_gen->to<Ast>(), *expr_type, is_optional, is_fallible, *error_type)
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::GenExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is no expression, then now ork needs to be done.
    if (expr == nullptr) return;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *tok_gen, *sm, true, true, false, false, false, false, meta);

    // If the value is non-symbolic, then there is no borrow logic to implement, so return.
    auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*expr);
    if (sym == nullptr) { return; }

    if (conv == nullptr) {
        // Ensure that attributes aren't being moved off of a borrowed value and that pins are maintained. Mark the move
        // or partial move of the argument.
        analyse::utils::mem_utils::validate_symbol_memory(
            *expr, *tok_gen, *sm, false, false, true, true, true, true, meta);
    }

    else if (*conv == ConventionTag::MUT and not sym->is_mutable) {
        // Check the argument's symbol is mutable, if the symbol exists.
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidMutationError>()
            .with_args(*expr, *conv, *std::get<0>(sym->memory_info->ast_initialization))
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::GenExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the generator environment.
    const auto coro = meta->enclosing_function_scope->ast->to<CoroutinePrototypeAst>();
    const auto llvm_gen_env = coro->llvm_gen_env;
    SPP_ASSERT(llvm_gen_env != nullptr);

    const auto [_, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *m_gen_type, *sm->current_scope, *m_gen_type, "gen");
    const auto yielded_type = expr ? expr->infer_type(sm, meta) : nullptr;
    const auto is_exception = yielded_type and analyse::utils::type_utils::symbolic_eq(
        *yield_type, *yielded_type, *sm->current_scope, *sm->current_scope);
    const auto gen_env_type = llvm::PointerType::get(*ctx->context, 0);

    // Generate the value and store it in the generator environment.
    if (expr != nullptr and not is_exception) {
        ctx->builder.CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), static_cast<std::uint8_t>(codegen::CoroutineState::VARIABLE)),
            ctx->builder.CreateStructGEP(gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::STATE)));

        ctx->builder.CreateStore(
            expr->stage_10_code_gen_2(sm, meta, ctx),
            ctx->builder.CreateStructGEP(gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::YIELD_SLOT)));
    }

    // Generate the exception and store it in the generator environment.
    else if (is_exception) {
        ctx->builder.CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), static_cast<std::uint8_t>(codegen::CoroutineState::EXCEPTION)),
            ctx->builder.CreateStructGEP(gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::STATE)));

        ctx->builder.CreateStore(
            expr->stage_10_code_gen_2(sm, meta, ctx),
            ctx->builder.CreateStructGEP(gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::ERROR_SLOT)));
    }

    // No expression => store "no value" state.
    else {
        ctx->builder.CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), static_cast<std::uint8_t>(codegen::CoroutineState::NO_VALUE)),
            ctx->builder.CreateStructGEP(gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::STATE)));

        ctx->builder.CreateStore(
            llvm::UndefValue::get(llvm::Type::getVoidTy(*ctx->context)),
            ctx->builder.CreateStructGEP(gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::YIELD_SLOT)));
    }

    ctx->builder.CreateRetVoid();

    // Continuation block.
    const auto cont_bb = llvm::BasicBlock::Create(*ctx->context, "gen.cont", ctx->builder.GetInsertBlock()->getParent());
    ctx->yield_continuations.push_back(cont_bb);
    ctx->builder.SetInsertPoint(cont_bb);

    // If this gen expression was "sent" a value, return it. Read off of the "send" slot.
    const auto send_slot_ptr = ctx->builder.CreateStructGEP(
        gen_env_type, llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::SEND_SLOT));
    return ctx->builder.CreateLoad(
        ctx->builder.getInt8Ty(), send_slot_ptr);
}


auto spp::asts::GenExpressionAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Get the "Send" generic type parameter from the generator type.
    auto send_type = m_gen_type->type_parts().back()->generic_arg_group->type_at("Send")->val;
    return send_type;
}
