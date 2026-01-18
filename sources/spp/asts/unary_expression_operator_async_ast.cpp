module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_operator_async_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;
import spp.utils.uid;


spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
    decltype(tok_async) &&tok_async) :
    tok_async(std::move(tok_async)) {
}


spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_start() const
    -> std::size_t {
    return tok_async->pos_start();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_end() const
    -> std::size_t {
    return tok_async->pos_end();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<UnaryExpressionOperatorAsyncAst>(
        ast_clone(tok_async));
}


spp::asts::UnaryExpressionOperatorAsyncAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_async).append(" ");
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the right-hand-side is a function call expression.
    if (const auto rhs = meta->unary_expression_rhs->to<PostfixExpressionAst>(); rhs == nullptr or not rhs->op->to<PostfixExpressionOperatorFunctionCallAst>()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppAsyncTargetNotFunctionCallError>()
            .with_args(*tok_async, *this)
            .raises_from(sm->current_scope);
    }
    else {
        // Mark the function call as async.
        rhs->op->to<PostfixExpressionOperatorFunctionCallAst>()->mark_as_async(this);
    }
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // We need a "Fut[T]" object to work with immediately.
    const auto uid = spp::utils::generate_uid(this);
    const auto fut_type = infer_type(sm, meta);
    const auto fut_type_sym = sm->current_scope->get_type_symbol(fut_type);
    const auto llvm_fut_type = codegen::llvm_type(*fut_type_sym, ctx);

    // Allocate the future onto the stack and set the initial state.
    const auto fut_alloca = ctx->builder.CreateAlloca(llvm_fut_type, nullptr, "async.fut.alloca" + uid);
    const auto fut_state_ptr = ctx->builder.CreateStructGEP(llvm_fut_type, fut_alloca, 0, "async.fut.state_ptr" + uid);
    const auto fut_state = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), 0);
    ctx->builder.CreateStore(fut_state, fut_state_ptr);

    // Generate the async closure function and set it in the future.
    {
        const auto fut_closure_type = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*ctx->context),
            {llvm::PointerType::get(*ctx->context, 0)}, false);

        const auto fut_closure = llvm::Function::Create(
            fut_closure_type, llvm::Function::InternalLinkage,
            "async.fut.closure" + uid, ctx->module.get());

        // Create the entry block for the closure.
        const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "async.fut.closure.entry" + uid, fut_closure);
        ctx->builder.SetInsertPoint(entry_bb);

        // Generate the function call expression inside the closure.
        meta->save();
        meta->prevent_auto_generator_resume = true;
        const auto fut_val = meta->unary_expression_rhs->stage_11_code_gen_2(sm, meta, ctx);
        meta->restore();

        // Get the function calls type information.
        const auto rhs_type = meta->unary_expression_rhs->infer_type(sm, meta);
        const auto fut_param = fut_closure->getArg(0);

        // Set the future's state to completed.
        const auto fut_state_ptr_in_closure = ctx->builder.CreateStructGEP(llvm_fut_type, fut_param, 0, "async.fut.state_ptr.in_closure" + uid);
        const auto fut_state_completed = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), 1);
        ctx->builder.CreateStore(fut_state_completed, fut_state_ptr_in_closure);

        // Set the value in the future (field 1).
        const auto fut_value_ptr_in_closure = ctx->builder.CreateStructGEP(llvm_fut_type, fut_param, 1, "async.fut.value_ptr.in_closure" + uid);
        if (not fut_val->getType()->isVoidTy()) { ctx->builder.CreateStore(fut_val, fut_value_ptr_in_closure); }

        // Return from the closure.
        ctx->builder.CreateRetVoid();

        // Next, we need to call the internal spawn function with the closure and future.
        const auto spawn_func = codegen::create_async_spawn_func(ctx, *fut_type_sym);
        ctx->builder.SetInsertPoint(ctx->builder.GetInsertBlock());
        ctx->builder.CreateCall(spawn_func, {fut_closure, fut_alloca});
    }

    // Return the future value.
    const auto fut_loaded = ctx->builder.CreateLoad(llvm_fut_type, fut_alloca, "async.fut.loaded" + uid);
    return fut_loaded;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Wrap the function call inside a "Future" type.
    auto inner_type = meta->unary_expression_rhs->infer_type(sm, meta);
    auto future_type = generate::common_types::future_type(tok_async->pos_start(), std::move(inner_type));
    future_type->stage_7_analyse_semantics(sm, meta);
    return future_type;
}
