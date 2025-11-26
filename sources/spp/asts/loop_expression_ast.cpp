#include <spp/analyse/errors/semantic_error.ixx>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/loop_condition_boolean_ast.hpp>
#include <spp/asts/loop_condition_iterable_ast.hpp>
#include <spp/asts/loop_else_statement_ast.hpp>
#include <spp/asts/loop_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>


spp::asts::LoopExpressionAst::LoopExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(cond) &&cond,
    decltype(body) &&body,
    decltype(else_block) &&else_block) :
    PrimaryExpressionAst(),
    tok_loop(std::move(tok_loop)),
    cond(std::move(cond)),
    body(std::move(body)),
    else_block(std::move(else_block)) {
}


spp::asts::LoopExpressionAst::~LoopExpressionAst() = default;


auto spp::asts::LoopExpressionAst::pos_start() const
    -> std::size_t {
    return tok_loop->pos_start();
}


auto spp::asts::LoopExpressionAst::pos_end() const
    -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::LoopExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopExpressionAst>(
        ast_clone(tok_loop),
        ast_clone(cond),
        ast_clone(body),
        ast_clone(else_block));
}


spp::asts::LoopExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_loop).append(" ");
    SPP_STRING_APPEND(cond).append(" ");
    SPP_STRING_APPEND(body).append("\n");
    SPP_STRING_APPEND(else_block);
    SPP_STRING_END;
}


auto spp::asts::LoopExpressionAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_loop).append(" ");
    SPP_PRINT_APPEND(cond).append(" ");
    SPP_PRINT_APPEND(body).append("\n");
    SPP_PRINT_APPEND(else_block);
    SPP_PRINT_END;
}


auto spp::asts::LoopExpressionAst::m_codegen_condition_bool(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) const
    -> llvm::Value* {
    /*
    let x = loop cond { body } else { else_block }

    br label %loop.cond

    loop.cond:
        %cond_val = ... ; bool
        br i1 %cond_val, label %loop.body, label %loop.else

    loop.body:
        %body_val = ... ; body codegen value
        br label %loop.cond

    loop.else:
        %else_val = ... ; else block result
        br label %loop.exit

    loop.exit:
        %result = phi <type> [ %else_val, %loop.else ]
     */

    // Create the building blocks.
    const auto fn = ctx->builder.GetInsertBlock()->getParent();
    const auto cond_bb = llvm::BasicBlock::Create(ctx->context, "loop.cond", fn);
    const auto body_bb = llvm::BasicBlock::Create(ctx->context, "loop.body");
    const auto else_bb = else_block ? llvm::BasicBlock::Create(ctx->context, "loop.else") : nullptr;
    const auto exit_bb = llvm::BasicBlock::Create(ctx->context, "loop.exit");

    // Jump to the cond block, and insert from there.
    ctx->builder.CreateBr(cond_bb);
    ctx->builder.SetInsertPoint(cond_bb);

    // Generate the condition.
    const auto llvm_cond = cond->stage_10_code_gen_2(sm, meta, ctx);
    const auto false_target = else_bb ? else_bb : exit_bb;
    ctx->builder.CreateCondBr(llvm_cond, body_bb, false_target);

    // Attach the remaining blocks.
    fn->insert(fn->end(), body_bb);
    ctx->builder.SetInsertPoint(body_bb);

    // Generate the body value and resume the loop (back to the check).
    const auto llvm_body = body->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateBr(cond_bb);

    // Handle the else block if it exists.
    auto llvm_else = static_cast<llvm::Value*>(nullptr);
    auto else_end_bb = static_cast<llvm::BasicBlock*>(nullptr);
    if (else_bb != nullptr) {
        fn->insert(fn->end(), else_bb);
        ctx->builder.SetInsertPoint(else_bb);
        llvm_else = else_block->stage_10_code_gen_2(sm, meta, ctx);
        ctx->builder.CreateBr(exit_bb);
        else_end_bb = ctx->builder.GetInsertBlock();
    }

    // Exit block and phi node.
    fn->insert(fn->end(), exit_bb);
    ctx->builder.SetInsertPoint(exit_bb);
    const auto phi_type = llvm_body->getType();
    const auto phi = ctx->builder.CreatePHI(phi_type, else_bb ? 2 : 1, "loop.exit.phi");
    phi->addIncoming(llvm_body, body_bb);
    else_bb
        ? phi->addIncoming(llvm_else, else_end_bb)
        : phi->addIncoming(llvm::Constant::getNullValue(phi_type), cond_bb);

    return phi;
}


auto spp::asts::LoopExpressionAst::m_codegen_condition_iter(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) const
    -> llvm::Value* {

    /*
    # loop y in range_coro() { y * 2 } else { -1 };
    # ============================================

    br label %loop.check

    loop.check:
        call void @llvm.coro.resume(ptr %iter)
        %done = call i1 @llvm.coro.done(ptr %iter)
        br i1 %done, label %loop.else, label %loop.body

    loop.body:
        %yield = call i32 @coro.yield.load(ptr %iter)
        %mul = mul i32 %yield, 2
        br label %loop.check

    loop.else:
        %elseval = add i32 -1, 0
        br label %loop.exit

    loop.exit:
        %phi = phi i32 [ %mul, %loop.body ], [ %elseval, %loop.else ]
        call void @llvm.coro.destroy(ptr %iter)
     */

    const auto fn = ctx->builder.GetInsertBlock()->getParent();

    // Evaluate the iterable (coroutine handle).
    const auto iterable_val = cond->stage_10_code_gen_2(sm, meta, ctx);
    const auto coro_handle = iterable_val;

    // Prepare the blocks that form the loop.
    const auto cond_bb = llvm::BasicBlock::Create(ctx->context, "loop.check", fn);
    const auto body_bb = llvm::BasicBlock::Create(ctx->context, "loop.body");
    const auto else_bb = else_block ? llvm::BasicBlock::Create(ctx->context, "loop.else") : nullptr;
    const auto exit_bb = llvm::BasicBlock::Create(ctx->context, "loop.exit");

    // Jump to the cond block, and insert from there.
    ctx->builder.CreateBr(cond_bb);
    ctx->builder.SetInsertPoint(cond_bb);

    // Resume the coroutine to get the next value.
    const auto coro_resume_decl = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::coro_resume);
    ctx->builder.CreateCall(coro_resume_decl, {coro_handle});

    // Check if the coroutine is done.
    const auto coro_done_decl = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::coro_done);
    const auto is_done = ctx->builder.CreateCall(coro_done_decl, {coro_handle}, "loop.is_done"); // todo: change this to an "iter" block-like analysis
    const auto false_target = else_bb ? else_bb : exit_bb;
    ctx->builder.CreateCondBr(is_done, false_target, body_bb);

    // Handle the body block, and insert from there.
    fn->insert(fn->end(), body_bb);
    ctx->builder.SetInsertPoint(body_bb);

    // Load the yielded value. Todo: hook into "iter" to extract value from the coroutine.
    // const auto coro_yield_load_decl = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::coro_yield_load);
    // const auto yielded_value = ctx->builder.CreateCall(coro_yield_load_decl, {coro_handle}, "loop.yielded_value");

    // Generate the body value and resume the loop (back to the check).
    const auto llvm_body = body->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateBr(cond_bb);

    // Handle the else block if it exists.
    auto llvm_else = static_cast<llvm::Value*>(nullptr);
    auto else_end_bb = static_cast<llvm::BasicBlock*>(nullptr);
    if (else_bb != nullptr) {
        fn->insert(fn->end(), else_bb);
        ctx->builder.SetInsertPoint(else_bb);
        llvm_else = else_block->stage_10_code_gen_2(sm, meta, ctx);
        ctx->builder.CreateBr(exit_bb);
        else_end_bb = ctx->builder.GetInsertBlock();
    }

    // Exit block and phi node.
    fn->insert(fn->end(), exit_bb);
    ctx->builder.SetInsertPoint(exit_bb);
    const auto phi_type = llvm_body->getType();
    const auto phi = ctx->builder.CreatePHI(phi_type, else_bb ? 2 : 1, "loop.exit.phi");
    phi->addIncoming(llvm_body, body_bb);
    else_bb
        ? phi->addIncoming(llvm_else, else_end_bb)
        : phi->addIncoming(llvm::Constant::getNullValue(phi_type), cond_bb);

    // Cleanup the coroutine.
    const auto coro_destroy_decl = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::coro_destroy);
    ctx->builder.CreateCall(coro_destroy_decl, {coro_handle});
    return phi;
}


auto spp::asts::LoopExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create the loop scope.
    auto scope_name = analyse::scopes::ScopeBlockName("<loop-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    cond->stage_7_analyse_semantics(sm, meta);

    // Set the loop level information into the "meta" object.
    meta->save();
    meta->current_loop_depth += 1;
    meta->current_loop_ast = this;
    body->stage_7_analyse_semantics(sm, meta);
    if (meta->loop_return_types->contains(meta->current_loop_depth - 1)) {
        m_loop_exit_type_info = (*meta->loop_return_types)[meta->current_loop_depth - 1];
    }
    meta->restore();

    // Analyse the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_7_analyse_semantics(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move into the loop scope.
    sm->move_to_next_scope();

    // Check twice so that invalidation fails on the second loop.
    // Todo: use the "reset" on "sm" like in TypeStatementAst?
    auto tm = ScopeManager(sm->global_scope, sm->current_scope);
    tm.reset(sm->current_scope, sm->m_it);
    for (auto &m : {sm, &tm}) {
        meta->save();
        meta->loop_double_check_active = m == &tm;
        cond->stage_8_check_memory(m, meta);
        meta->restore();

        body->stage_8_check_memory(m, meta);
    }

    // Check the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_8_check_memory(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code based on the condition type.
    return ast_cast<LoopConditionBooleanAst>(cond.get())
               ? m_codegen_condition_bool(sm, meta, ctx)
               : m_codegen_condition_iter(sm, meta, ctx);
}


auto spp::asts::LoopExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the loop's exit type (or Void if there are no exits fro inside the loop).
    auto [exit_expr, loop_type, _] = m_loop_exit_type_info.has_value()
                                         ? *m_loop_exit_type_info
                                         : std::make_tuple(nullptr, generate::common_types::void_type(pos_start()), nullptr);
    exit_expr = exit_expr ? exit_expr : this;

    // Check the else block's type is the same as the loop exit type.
    if (else_block != nullptr and not meta->ignore_missing_else_branch_for_inference) {
        const auto else_type = else_block->infer_type(sm, meta);
        if (not analyse::utils::type_utils::symbolic_eq(*loop_type, *else_type, *sm->current_scope, *sm->current_scope)) {
            const auto final_member = else_block->body->final_member();
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *exit_expr, *loop_type, *final_member, *else_type).with_scopes({sm->current_scope}).raise();
        }
    }

    // If the return type is "Void", but the condition is comptime "true", then the loop is infinite.
    // Todo: change this to count the number of "exit" statements, as "exit" (void) is possible.
    if (analyse::utils::type_utils::symbolic_eq(*loop_type, *generate::common_types_precompiled::VOID, *sm->current_scope, *sm->current_scope)) {
        if (ast_cast<LoopConditionBooleanAst>(cond.get())) {
            // Non-symbolic expressions, or compile time symbolic boolean expressions, can never change (infinite loop)
            const auto is_symbolic = ast_cast<IdentifierAst>(cond.get());
            if (not is_symbolic or sm->current_scope->get_var_symbol(is_symbolic->shared_from_this())->memory_info->ast_comptime != nullptr) {
                loop_type = generate::common_types::never_type(pos_start());
            }
        }
    }

    // Return the loop type.
    return loop_type;
}
