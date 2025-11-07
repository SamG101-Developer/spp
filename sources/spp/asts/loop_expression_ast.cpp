#include <spp/analyse/errors/semantic_error.hpp>
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
    // Create the building blocks.
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto cond_bb = llvm::BasicBlock::Create(ctx->context, "loop.cond", func);
    const auto body_bb = llvm::BasicBlock::Create(ctx->context, "loop.body");
    const auto exit_bb = llvm::BasicBlock::Create(ctx->context, "loop.exit");
    const auto else_bb = else_block ? llvm::BasicBlock::Create(ctx->context, "loop.else") : nullptr;

    // Jump to the condition block.
    ctx->builder.CreateBr(cond_bb);
    ctx->builder.SetInsertPoint(cond_bb);

    // Generate the condition.
    const auto llvm_cond = cond->stage_10_code_gen_2(sm, meta, ctx);
    const auto false_target = else_bb ? else_bb : exit_bb;
    ctx->builder.CreateCondBr(llvm_cond, body_bb, false_target);

    // Attach the remaining blocks.
    func->insert(func->end(), body_bb);
    func->insert(func->end(), exit_bb);
    if (else_bb) { func->insert(func->end(), else_bb); }

    // Handle the body block.
    ctx->builder.SetInsertPoint(body_bb);
    const auto llvm_body = body->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateBr(cond_bb);

    // Handle the else block if it exists.
    auto llvm_else = static_cast<llvm::Value*>(nullptr);
    if (else_bb) {
        ctx->builder.SetInsertPoint(else_bb);
        llvm_else = else_block->stage_10_code_gen_2(sm, meta, ctx);
        ctx->builder.CreateBr(exit_bb);
    }

    // Handle the exit block.
    ctx->builder.SetInsertPoint(exit_bb);
    const auto phi = ctx->builder.CreatePHI(llvm_body->getType(), else_bb ? 2 : 1, "loop.exit.phi");

    // Add the incoming values to the phi node.
    phi->addIncoming(llvm_body, body_bb);
    if (else_bb) {
        phi->addIncoming(llvm_else, else_bb);
    }
    else {
        phi->addIncoming(llvm::Constant::getNullValue(llvm_body->getType()), cond_bb);
    }
    return phi;
}


auto spp::asts::LoopExpressionAst::m_codegen_condition_iter(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) const
    -> llvm::Value* {
    // // Evaluate the iterable with coroutine intrinsics.
    // const auto llvm_iter = cond->stage_10_code_gen_2(sm, meta, ctx);
    // const auto llvm_coro_handle = llvm_iter;
    //
    // // Create the building blocks.
    // const auto func = ctx->builder.GetInsertBlock()->getParent();
    // const auto entry_bb = ctx->builder.GetInsertBlock();
    // const auto pre_bb = llvm::BasicBlock::Create(ctx->context, "loop.pre", func);
    // const auto body_bb = llvm::BasicBlock::Create(ctx->context, "loop.body", func);
    // const auto resume_bb = llvm::BasicBlock::Create(ctx->context, "loop.resume", func);
    // const auto done_bb = llvm::BasicBlock::Create(ctx->context, "loop.done", func);
    // const auto exit_bb = llvm::BasicBlock::Create(ctx->context, "loop.exit", func);
    // const auto else_bb = else_block ? llvm::BasicBlock::Create(ctx->context, "loop.else", func) : nullptr;
    //
    // // Jump to the pre block from the entry block.
    // ctx->builder.CreateBr(pre_bb);
    //
    // // Generate the iterable handle in the pre block.
    // ctx->builder.SetInsertPoint(pre_bb);
    // const auto hdl = cond->stage_10_code_gen_2(sm, meta, ctx);
    //
    // // First resume.
    // ctx->builder.CreateIntrinsic(llvm::Intrinsic::coro_resume, {}, {hdl});
    //
    // // Get the promise pointer.
    // const auto align_val = llvm::ConstantInt::get(ctx->context, llvm::APInt(32, 8));
    // const auto from_val = llvm::ConstantInt::get(ctx->context, llvm::APInt(1, 0));
    // const auto promise = ctx->builder.CreateIntrinsic(llvm::Intrinsic::coro_promise, {}, {hdl, align_val, from_val});
    //
    // // The pointer points to `struct { yielded_type val, i1 done }.
    // const auto value_ptr = ctx->builder.CreateStructGEP(promise->getType(), promise, 0, "iter.promise.value.ptr");
    // const auto done_ptr = ctx->builder.CreateStructGEP(promise->getType(), promise, 1, "iter.promise.done.ptr");
    //
    // // Load done.
    // const auto is_done = ctx->builder.CreateLoad(done_ptr->getType(), done_ptr, "iter.done");
    //
    // // False target for the first resume (else_bb or done_bb).
    // const auto false_target = else_bb ? else_bb : done_bb;
    // ctx->builder.CreateCondBr(is_done, false_target, body_bb);
    //
    // // Handle the body block.
    // ctx->builder.SetInsertPoint(body_bb);
    //
    // // Load the yielded value.
    // const auto llvm_value = ctx->builder.CreateLoad(value_ptr->getType(), value_ptr, "iter.yielded.value");
    //
    // // Declare the loop variable (LocalVariableAst's codegen will have alloca the variable).
    // cond->stage_10_code_gen_2(sm, meta, ctx);
    (void)sm;
    (void)meta;
    (void)ctx;
    return nullptr;
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
