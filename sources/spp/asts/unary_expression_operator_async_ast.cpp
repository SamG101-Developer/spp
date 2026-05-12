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

SPP_MOD_BEGIN
spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
    decltype(TokAsync) &&tok_async) :
    TokAsync(std::move(tok_async)) {
}

spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;

auto spp::asts::UnaryExpressionOperatorAsyncAst::PosStart() const
    -> std::size_t {
    // Use the "async" token.
    return TokAsync->PosStart();
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::PosEnd() const
    -> std::size_t {
    // Use the "async" token.
    return TokAsync->PosEnd();
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<UnaryExpressionOperatorAsyncAst>(
        AstClone(TokAsync));
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokAsync).append(" ");
    SPP_STRING_END;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppAsyncTargetNotFunctionCallError;

    // Check the right-hand-side is a function call expression.
    const auto rhs = meta->UnaryExpressionRhs->To<PostfixExpressionAst>();
    RaiseIf<SppAsyncTargetNotFunctionCallError>(
        rhs == nullptr or rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>() == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*TokAsync, *meta->UnaryExpressionRhs));

    // Mark the function call as async.
    rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>()->mark_as_async(this);
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // We need a "Fut[T]" object to work with immediately.
    const auto uid = spp::utils::Uid(this);
    const auto fut_type = InferType(sm, meta);
    const auto fut_type_sym = sm->CurrentScope->GetTypeSymbol(fut_type);
    const auto llvm_fut_type = codegen::llvm_type(*fut_type_sym, ctx);

    // Allocate the future onto the stack and set the initial state.
    const auto fut_alloca = ctx->Builder.CreateAlloca(llvm_fut_type, nullptr, "async.fut.alloca" + uid);
    const auto fut_state_ptr = ctx->Builder.CreateStructGEP(llvm_fut_type, fut_alloca, 0, "async.fut.state_ptr" + uid);
    const auto fut_state = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), 0);
    ctx->Builder.CreateStore(fut_state, fut_state_ptr);

    // Generate the async closure function and set it in the future.
    {
        const auto fut_closure_type = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*ctx->Context),
            {llvm::PointerType::get(*ctx->Context, 0)}, false);

        const auto fut_closure = llvm::Function::Create(
            fut_closure_type, llvm::Function::InternalLinkage,
            "async.fut.closure" + uid, ctx->Module.get());

        // Create the entry block for the closure.
        const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "async.fut.closure.entry" + uid, fut_closure);
        ctx->Builder.SetInsertPoint(entry_bb);

        // Generate the function call expression inside the closure.
        meta->Save();
        meta->PreventAutoGeneratorResume = true;
        const auto fut_val = meta->UnaryExpressionRhs->Stage11_CodeGen(sm, meta, ctx);
        meta->Restore();

        // Get the function calls type information.
        const auto rhs_type = meta->UnaryExpressionRhs->InferType(sm, meta);
        const auto fut_param = fut_closure->getArg(0);

        // Set the future's state to completed.
        const auto fut_state_ptr_in_closure = ctx->Builder.CreateStructGEP(llvm_fut_type, fut_param, 0, "async.fut.state_ptr.in_closure" + uid);
        const auto fut_state_completed = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), 1);
        ctx->Builder.CreateStore(fut_state_completed, fut_state_ptr_in_closure);

        // Set the value in the future (field 1).
        const auto fut_value_ptr_in_closure = ctx->Builder.CreateStructGEP(llvm_fut_type, fut_param, 1, "async.fut.value_ptr.in_closure" + uid);
        if (not fut_val->getType()->isVoidTy()) { ctx->Builder.CreateStore(fut_val, fut_value_ptr_in_closure); }

        // Return from the closure.
        ctx->Builder.CreateRetVoid();

        // Next, we need to call the internal spawn function with the closure and future.
        const auto spawn_func = codegen::create_async_spawn_func(ctx, *fut_type_sym);
        ctx->Builder.SetInsertPoint(ctx->Builder.GetInsertBlock());
        ctx->Builder.CreateCall(spawn_func, {fut_closure, fut_alloca});
    }

    // Return the future value.
    const auto fut_loaded = ctx->Builder.CreateLoad(llvm_fut_type, fut_alloca, "async.fut.loaded" + uid);
    return fut_loaded;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    //
    using generate::common_types::FutureType;

    // Wrap the function call inside a "Future" type.
    auto inner_type = meta->UnaryExpressionRhs->InferType(sm, meta);
    auto future_type = FutureType(TokAsync->PosStart(), std::move(inner_type));
    future_type->Stage7_AnalyseSemantics(sm, meta);
    return future_type;
}

SPP_MOD_END
